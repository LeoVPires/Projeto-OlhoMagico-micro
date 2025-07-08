import datetime
import os # Importar para verificar se o arquivo existe e para operações de arquivo

class VisitLogger:
    def __init__(self, log_file="visits.log", max_entries=200):
        self.log_file = log_file
        self.max_entries = max_entries
        # Nao precisamos carregar todos os logs para 'self.logs' na inicializacao se sempre lermos do arquivo
        # self.logs = self._load_logs() # Removido

    def add_log(self, log_entry):
        try:
            # Abrir o arquivo em modo append ('a') para adicionar o novo log
            with open(self.log_file, 'a') as f:
                f.write(log_entry + '\n')
            
            # Após adicionar, verificar se excedeu o limite e reescrever se necessario
            self._prune_log_file()

        except Exception as e:
            print(f"Erro ao adicionar log: {e}")

    def _prune_log_file(self):
        """
        Mantém o arquivo de log com no máximo self.max_entries linhas.
        Lê todas as linhas, mantém as 200 mais recentes e reescreve o arquivo.
        """
        try:
            if not os.path.exists(self.log_file):
                return # Arquivo nao existe, nao ha nada para podar

            with open(self.log_file, 'r') as f:
                lines = f.readlines()
            
            if len(lines) > self.max_entries:
                # Mantém apenas as 'max_entries' linhas mais recentes
                pruned_lines = lines[-self.max_entries:]
                with open(self.log_file, 'w') as f: # Abrir em modo 'w' para sobrescrever
                    f.writelines(pruned_lines)
                print(f"Arquivo de log podado para {self.max_entries} entradas.")
        except Exception as e:
            print(f"Erro ao podar arquivo de log: {e}")

    def get_daily_logs(self):
        today = datetime.date.today().strftime("%d/%m/%Y")
        daily_entries = []
        try:
            if not os.path.exists(self.log_file):
                return "Ainda não houve nenhuma visita hoje." # Retorna imediatamente se o arquivo nao existe

            with open(self.log_file, 'r') as f:
                for line in f:
                    entry = line.strip()
                    if today in entry:
                        daily_entries.append(entry)
        except Exception as e:
            print(f"Erro ao ler logs diarios: {e}")
            return "Erro ao carregar logs diários."
            
        return "\n".join(daily_entries) if daily_entries else "Ainda não houve nenhuma visita hoje."

    def get_all_logs_file(self):
        """Retorna o caminho do arquivo de log."""
        return self.log_file