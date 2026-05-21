Smart Nebulizer Timer (ESP32)

Sistema de temporização para inaladores/nebulizadores utilizando ESP32, criado para evitar superaquecimento e desligar automaticamente o aparelho após um tempo configurado pelo próprio usuário.
Visando a problemática de que em hospitais e nas próprias casas, algumas pessoas não sabem a recomendação de tempo limite de utilização do aparelho, então esse sistema aumenta a vida útil desses aparelhos.

O projeto conta com controle via navegador, botão físico e comunicação em tempo real utilizando WebSocket.

Funcionalidades
-Controle do inalador via relé (GPIO 26)
-Timer configurável de 5 segundos até 5 minutos
-Desligamento automático ao fim do tempo
-Alerta sonoro no buzzer (GPIO 4) quando faltam 10 segundos
-3 bipes finais antes de desligar
-Botão físico no GPIO 34 para iniciar/parar localmente
-Interface Web em tempo real
-Proteção contra funcionamento contínuo
-Interface Web

Fiz um projeto médio, para que a implementação não seja muito complicada, mas ainda há como simplificar, retirando por exemplo:
Alerta sonoro no buzzer (GPIO 4) quando faltam 10 segundos
3 bipes finais antes de desligar

A interface pode ser acessada de qualquer dispositivo conectado na mesma rede Wi-Fi da ESP32.

Recursos:

-Countdown em tempo real
-Anel de progresso animado
-Slider de duração
-Atalhos rápidos de tempo
-Botões iniciar/parar
-Status do sistema:
  -idle
  -em uso
  -concluído
  -offline
-Log de eventos com timestamps
-Tecnologias utilizadas
-Hardware
-ESP32 DevKit V1
-Módulo Relé 5V Optoacoplado
-Buzzer Ativo 5V
-Software
-Arduino IDE
-HTML/CSS/JavaScript
-WebSocket
-Estrutura

Pretendo colocar esse projeto em prática, mas por enquanto deixo aqui apenas o software e:
Caso queira fazer na prática recomendo utilizar IA para orientações mais específicas e também tutoriais do youtube orientados a tomadas inteligentes, pois utilizam a mesma lógica.

Como usar o software:
-Faça upload do firmware para a ESP32
-Conecte a ESP32 na rede Wi-Fi
-Abra o navegador e acesse o IP da placa:
-http://192.168.x.x
-Configure o tempo e inicie o sistema

Observações:

Desenvolvi esse sistema para estudos e uso pessoal caso necessário

Tenha cuidado com cargas AC (127/220V). O ideal mesmo é utilizar relés optoacoplados, fusível e caixa isolante.

Possívels futuras melhorias:
Sensor de temperatura
Integração com Home Assistant

Autor

🌱Gustavo Oliveira🪐
