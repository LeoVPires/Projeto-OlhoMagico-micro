import telegram
from telegram.ext import (
    Application,
    ApplicationBuilder,
    CommandHandler,
    ContextTypes
)
import asyncio
import threading
import time
from datetime import datetime
import requests

# --- Configurações ---
TOKEN = "7691898679:AAEdlE-BD4rFwxMUczeC8GnjE84UD3z-b_M"
STREAM_URL = "http://192.168.1.78/stream" # IP da ESP32-CAM

# --- NOVO: Configuração para o segundo ESP32 (Alerta) ---
ALERT_ESP_IP = "192.168.1.81" # <--- SUBSTITUA PELO IP DO SEGUNDO ESP32 (Servidor de Alerta)
# ALERT_ENDPOINT base para adicionar o parametro 'pessoas'
ALERT_BASE_ENDPOINT = f"http://{ALERT_ESP_IP}/acionar_alerta"

# Variáveis globais para o Application e chat_id_admin
application = None
chat_id_admin = None

# Importa as classes dos outros arquivos
from camera_processor import CameraProcessor
from visit_logger import VisitLogger

# --- Instâncias das classes ---
camera_proc = CameraProcessor(STREAM_URL)
camera_proc.load_known_faces()
logger = VisitLogger()

# --- Funções do Bot Telegram (Handlers) ---
# ... (Mantenha todos os seus handlers de comando como estão) ...
async def start(update: telegram.Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    global chat_id_admin
    chat_id_admin = update.effective_chat.id
    await update.message.reply_text('Olá! Eu sou o seu bot de segurança. Para começar, use /help para ver os comandos disponíveis.')

async def help_command(update: telegram.Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    await update.message.reply_text("""
Comandos disponíveis:
/visitas - Mostra todas as visitas do dia.
/visitas_todas - Envia o arquivo de texto com todos os 200 logs.
/cadastrar <nome> - O reconhecimento facial está desativado, não é possível cadastrar.
/remover_cadastro <nome> - O reconhecimento facial está desativado, não é possível remover.
    """)

async def visitas_command(update: telegram.Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    logs_hoje = logger.get_daily_logs()
    await update.message.reply_text(logs_hoje)

async def visitas_todas_command(update: telegram.Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    try:
        with open(logger.get_all_logs_file(), 'rb') as f:
            await update.message.reply_document(f)
    except FileNotFoundError:
        await update.message.reply_text("Nenhum log disponível ainda.")

async def cadastrar_command(update: telegram.Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    await update.message.reply_text("O reconhecimento facial está desativado no momento. Não é possível cadastrar rostos.")

async def remover_cadastro_command(update: telegram.Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    await update.message.reply_text("O reconhecimento facial está desativado no momento. Não é possível remover cadastros.")

async def error_handler(update: telegram.Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    print(f"Update '{update}' caused error '{context.error}'")

# --- Função de verificação da câmera em thread separada ---
async def check_camera_feed():
    global chat_id_admin
    # Este dicionario agora rastreia o tempo da ULTIMA NOTIFICACAO para cada pessoa,
    # nao o tempo da ultima deteccao.
    last_notified_time = {} 
    # Cooldown para notificacoes no Telegram. Uma vez que uma pessoa e notificada,
    # ela nao sera notificada novamente ate que este tempo passe.
    telegram_notification_cooldown_seconds = 60 
    
    last_alert_trigger_time = 0 
    alert_trigger_cooldown_seconds = 10 

    while True:
        frame = camera_proc.get_frame()
        if frame is not None:
            detected_people, _, _ = camera_proc.detect_and_recognize_faces(frame)

            if detected_people:
                current_time = datetime.now()
                # Conta o número total de pessoas detectadas no frame, para o alerta físico.
                num_detected_people = len(detected_people) 

                # Prepare a lista de nomes para a mensagem do Telegram, incluindo TODOS os detectados
                # Esta lista sera usada para a mensagem, independentemente do cooldown individual.
                all_person_info = []
                for person in detected_people:
                    name = person['name']
                    confidence = person['confidence']
                    all_person_info.append(f"{name}({confidence}%)") # <--- Inclui todos aqui

                # Verifica se a notificacao do Telegram deve ser enviada
                # Ela será enviada se QUALQUER pessoa detectada nao tiver sido notificada
                # recentemente, ou se for a primeira deteccao.
                should_send_telegram_notification = False
                for person in detected_people:
                    name = person['name']
                    if name not in last_notified_time or \
                       (current_time - last_notified_time[name]).total_seconds() > telegram_notification_cooldown_seconds:
                        should_send_telegram_notification = True
                        # Atualiza o tempo da ultima notificacao para esta pessoa
                        last_notified_time[name] = current_time 
                
                if should_send_telegram_notification:
                    # --- Lógica de Notificação Telegram ---
                    if chat_id_admin and application:
                        message_time = current_time.strftime("%H:%M - %d/%m/%Y")
                        if num_detected_people == 1:
                            message = f"Existe 1 pessoa na porta: {all_person_info[0]} - {message_time}"
                        else:
                            # A mensagem agora lista todas as pessoas detectadas no frame
                            message = f"Existem {num_detected_people} pessoas na porta: {', '.join(all_person_info)} - {message_time}"
                        
                        try:
                            await application.bot.send_message(chat_id=chat_id_admin, text=message)
                            logger.add_log(message)
                        except telegram.error.Unauthorized:
                            print("Bot nao autorizado a enviar mensagem para o chat_id_admin. Verifique se o bot foi iniciado neste chat.")
                            chat_id_admin = None
                            print("chat_id_admin resetado para None.")
                        except Exception as e:
                            print(f"Erro ao enviar mensagem ou logar: {e}")

                # --- Lógica para Acionar Alerta no Segundo ESP32 ---
                # Envia o alerta físico apenas se o cooldown tiver passado
                # Note que esta logica e independente do cooldown do Telegram
                if (time.time() - last_alert_trigger_time) > alert_trigger_cooldown_seconds:
                    alert_url = f"{ALERT_BASE_ENDPOINT}?pessoas={num_detected_people}" 
                    try:
                        print(f"Enviando requisicao de alerta para {alert_url}...")
                        response = requests.get(alert_url, timeout=5)
                        response.raise_for_status()
                        print(f"Alerta acionado no ESP32! Resposta: {response.text}")
                        last_alert_trigger_time = time.time()
                    except requests.exceptions.RequestException as e:
                        print(f"Erro ao enviar requisicao de alerta para o ESP32: {e}")
                    except Exception as e:
                        print(f"Erro inesperado ao acionar alerta: {e}")
            
        await asyncio.sleep(1)

# --- Função principal para rodar o bot ---
def main():
    global application

    application = ApplicationBuilder().token(TOKEN).build()

    application.add_handler(CommandHandler("start", start))
    application.add_handler(CommandHandler("help", help_command))
    application.add_handler(CommandHandler("visitas", visitas_command))
    application.add_handler(CommandHandler("visitas_todas", visitas_todas_command))
    application.add_handler(CommandHandler("cadastrar", cadastrar_command))
    application.add_handler(CommandHandler("remover_cadastro", remover_cadastro_command))

    application.add_error_handler(error_handler)

    def run_async_in_thread():
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        loop.run_until_complete(check_camera_feed())

    camera_thread = threading.Thread(target=run_async_in_thread)
    camera_thread.daemon = True
    camera_thread.start()

    application.run_polling()

if __name__ == '__main__':
    main()