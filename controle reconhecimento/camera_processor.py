import cv2
import requests
import numpy as np
import json
import re
import time # Para usar time.sleep

class CameraProcessor:
    def __init__(self, stream_url):
        self.stream_url = stream_url
        self.face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')
        if self.face_cascade.empty():
            print("Erro: Não foi possível carregar o classificador Haar Cascade. Verifique o caminho.")
        self.known_faces = {}
        self.stream_response = None # Para armazenar a resposta da conexão de stream
        self.bytes_buffer = bytes() # Buffer para os dados do stream

    def _connect_to_stream(self):
        """Estabelece e mantém a conexão com o stream da câmera."""
        if self.stream_response:
            try:
                self.stream_response.close() # Fecha a conexão anterior se existir
                self.stream_response.raise_for_status() # Verifica se ainda está ok
            except Exception:
                pass # Ignora erros ao fechar uma conexão possivelmente morta
            self.stream_response = None

        print(f"Tentando conectar a: {self.stream_url}")
        try:
            self.stream_response = requests.get(self.stream_url, stream=True, timeout=30)
            self.stream_response.raise_for_status()
            print(f"Conexão bem-sucedida. Status: {self.stream_response.status_code}")

            content_type = self.stream_response.headers.get('Content-Type', '')
            match = re.search(r'boundary=(\S+)', content_type)
            if not match:
                print("Erro: Não foi possível encontrar o boundary no cabeçalho Content-Type.")
                self.stream_response = None # Marca como falha na conexão
                return None
            
            boundary = match.group(1).encode('utf-8')
            print(f"Content-Type recebido: {content_type}")
            print(f"Boundary do stream detectado: {boundary}")
            return boundary

        except requests.exceptions.ConnectionError as e:
            print(f"Erro de conexão com a ESPCam. Erro: {e}")
            self.stream_response = None
            return None
        except requests.exceptions.Timeout as e:
            print(f"Tempo limite excedido ao conectar à ESPCam. Erro: {e}")
            self.stream_response = None
            return None
        except requests.exceptions.HTTPError as e:
            print(f"Erro HTTP ao conectar à ESPCam: {e.response.status_code} - {e.response.reason}")
            self.stream_response = None
            return None
        except Exception as e:
            print(f"Erro inesperado ao conectar ao stream da ESPCam: {e}")
            self.stream_response = None
            return None

    def get_frame(self):
        if not self.stream_response: # Se não há conexão ativa, tenta estabelecer
            self.boundary = self._connect_to_stream()
            if not self.boundary:
                time.sleep(5) # Espera antes de tentar reconectar novamente
                return None
            self.bytes_buffer = bytes() # Limpa o buffer ao reconectar

        try:
            for chunk in self.stream_response.iter_content(chunk_size=1024):
                if not chunk: # Stream terminou ou não há mais dados
                    print("Stream de câmera encerrado ou vazio. Tentando reconectar...")
                    self.stream_response = None # Força reconexão
                    return None

                self.bytes_buffer += chunk

                a = self.bytes_buffer.find(self.boundary)
                if a != -1:
                    frame_segment = self.bytes_buffer[:a]
                    self.bytes_buffer = self.bytes_buffer[a + len(self.boundary):]

                    header_end_index = frame_segment.find(b'\r\n\r\n')
                    if header_end_index != -1:
                        jpg_data = frame_segment[header_end_index + 4:]
                        if not jpg_data:
                            print("Aviso: Dados JPG vazios após extração dos cabeçalhos.")
                            continue

                        frame = cv2.imdecode(np.frombuffer(jpg_data, dtype=np.uint8), cv2.IMREAD_COLOR)
                        if frame is None:
                            print(f"Erro: cv2.imdecode retornou None. Imagem JPG pode estar corrompida ou formato inválido. (Tamanho: {len(jpg_data)} bytes)")
                        else:
                            # print(f"Frame decodificado com sucesso! Dimensões: {frame.shape}") # Pode manter se quiser
                            return frame
            # Se o loop terminar sem retornar um frame (por exemplo, stream fechado pelo servidor)
            print("Loop de chunks do stream terminou inesperadamente. Tentando reconectar...")
            self.stream_response = None
            
        except requests.exceptions.ConnectionError as e:
            print(f"Erro de conexão no meio do stream com a ESPCam. Erro: {e}. Tentando reconectar...")
            self.stream_response = None # Força reconexão
        except requests.exceptions.ChunkedEncodingError as e:
            print(f"Erro de codificação chunked no stream. Erro: {e}. Tentando reconectar...")
            self.stream_response = None
        except Exception as e:
            print(f"Erro inesperado ao ler frame do stream: {e}. Tentando reconectar...")
            self.stream_response = None
            
        return None # Retorna None se não conseguir um frame válido ou se houver erro

    # ... (o restante da sua classe CameraProcessor permanece o mesmo) ...
    def detect_and_recognize_faces(self, frame):
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY) # Converte para escala de cinza
        
        # Detecta rostos usando o classificador Haar Cascade
        faces = self.face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))

        detected_people = []

        if len(faces) > 0:
            for (x, y, w, h) in faces:
                detected_people.append({
                    "name": "Desconhecido",
                    "confidence": 100, # A confiança aqui é da detecção, não do reconhecimento
                    "location": (y, x + w, y + h, x) # (top, right, bottom, left) para compatibilidade de formato
                })
        
        return detected_people, [], [] # Retorna detected_people, face_locations vazias, face_encodings vazias

    def register_face(self, name, face_encoding):
        print("Reconhecimento facial desativado. Não é possível cadastrar.")
        return False, "Reconhecimento facial desativado."

    def remove_face(self, name):
        print("Reconhecimento facial desativado. Não é possível remover cadastro.")
        return False, "Reconhecimento facial desativado."

    def load_known_faces(self, filename="known_faces.json"):
        print("Reconhecimento facial desativado. Rostos conhecidos não serão carregados.")
        self.known_faces = {}

    def save_known_faces(self, filename="known_faces.json"):
        print("Reconhecimento facial desativado. Rostos conhecidos não serão salvos.")