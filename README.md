# 🍓 Sistema de Irrigação Automática para Moranguinhos

Sistema de irrigação inteligente e automatizado desenvolvido em C para microcontrolador PIC, projetado especificamente para o cultivo de morangos. O sistema monitora continuamente as condições do solo e da água, mantendo controle rigoroso do pH para promover o crescimento saudável das plantas.

---

## ⚙️ Funcionalidades

### 💧 Monitoramento e Irrigação
- Monitora continuamente a umidade do solo
- Ativa a irrigação automaticamente quando a umidade cai abaixo de 40%
- Desliga o aspersor ao atingir 60% de umidade
- Se a umidade já estiver acima de 60% ao acionar irrigação forçada, irriga por apenas 10 segundos

### 🪣 Gerenciamento do Reservatório
- Prioriza o uso de água da chuva
- Aciona bomba para captação de água do rio quando necessário
- Três sensores de nível: nível adequado, nível alto e nível alto2
- Proteção automática contra transbordamento

### 🧪 Controle de pH
- Mantém o pH na faixa ideal para morangueiros: **5,5 a 6,5**
- Adiciona base se o pH estiver ácido, e ácido se estiver básico
- Dosagem ajustada automaticamente para pH muito fora da faixa
- Misturador acionado após cada adição para homogeneização
- Irrigação só é liberada após o pH estar dentro da faixa ideal

### 🔒 Segurança
- Botão de irrigação forçada com verificações de segurança mantidas
- Proteção contra encharcamento em chuvas intensas via válvula de escoamento

### 📟 Interface com Usuário
- Display LCD com informações em tempo real
- Exibe pH e umidade no modo de espera
- Informa a etapa em curso durante operação ("Bomba ligada", "Ajustando pH", "Irrigando")
- Alertas específicos para diferentes situações

---

## 🛠️ Tecnologias e Componentes

- **Linguagem:** C
- **Microcontrolador:** PIC (MPLab)
- **Sensores:** Umidade do solo, pH, nível de água (x3)
- **Atuadores:** Bomba, aspersor, válvula de escoamento, misturador
- **Interface:** Display LCD

---

## 📁 Estrutura do Repositório

```
├── MPLab/          # Código-fonte e projeto MPLab
├── Simulacao/      # Arquivos de simulação
├── Placa/          # Arquivos da placa de circuito
├── TF MicMic.pdf   # Relatório do projeto
└── TF micmic.mp4   # Vídeo de demonstração
```

---

## 📚 Contexto Acadêmico

Trabalho Final desenvolvido na UFSC — Campus Araranguá, Curso de Engenharia da Computação.
