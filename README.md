# Redes de Computadores - Internet Relay Chat

> Este trabalho pode ser acessado pelo [Github](https://github.com/natan-dot-com/Internet-Relay-Chat), [vídeo](https://youtu.be/FRpHRayL3GY).

### Autores

- Natan Henrique Sanches (11795680)
- Gabriel da Cunha Dertoni (11795717)
- Álvaro José Lopes (10873365)

## Introdução

Neste projeto, foi implementado um modelo de IRC (Internet Relay Chat) simples, baseado na interação entre diversos clientes e um servidor. O projeto foi feito com caráter pedagógico para a disciplina de Redes de Computadores, utilizando linguagem C++ e bibliotecas como `sys/socket.h`, `arpa/inet.h` e `netinet/in.h` (para permitir a transmissão de mensagens cliente-servidor), além da biblioteca `ncurses.h` (para criação da interface do cliente).

## Implementação

O servidor pôde ser implementado em uma única _thread_, através da utilização do sistema de gerenciamento de eventos em um descritor de arquivos (`poll`). Além disso, o cliente foi implementado com duas _threads_, encarregadas de enviar mensagens (_sender_) e receber mensagens (_receiver_) do servidor. A _thread sender_ envia mensagens por demanda ao servidor central, enquanto a _thread receiver_ constantemente lê o _buffer_ de mensagens e, se houver alguma ainda não entregue, ela é exibida na tela.

## Procedimentos de Execução

O projeto foi testado nos sistemas operacionais Debian 11 '_Bullseye_' e Windows 11, além de ter sido compilado (em ambos os casos) com G++11.

```bash
# Compila tanto o servidor, quanto o cliente.
make

# Roda o sevidor
./build/server/main

# Roda o client
./build/client/main <ip_do_servidor> 8080
#                                    ^^^^~~~ porta para se conectar.
```

## Comandos

Neste projeto, foram implementados os comandos do protocolo [RFC 1459](https://datatracker.ietf.org/doc/html/rfc1459). Os comandos requisitados no enunciado foram, portanto, codificados em função dos comandos do RFC 1459.

#### Lista de Comandos

|**Comando**|**Descrição**|**Permissão**|
|-----------|-------------|-------------|
|`/connect`|Estabelece a conexão com o servidor|Todos os usuários|
|`/quit`|Encerra a conexão do cliente com o servidor|Todos os usuários|
|`/ping`|Checa o estado da conexão com o servidor. Retorna `pong` se estiver conectado|Todos os usuários|
|`/join <Nome Canal>`|Entra (ou cria, se não existir) no canal `<NomeCanal>`|Todos os usuários|
|`/nickname <Apelido>`|Atribui um determinado apelido para o cliente autor|Todos os usuários|
|`/user <Apelido> <Nome Real>`|Atribui informações a um cliente com determinado apelido|Todos os usuários|
|`/kick <Apelido>`|Expulsa um determinado usuário do servidor|Somente administrador|
|`/mute <Apelido>`|Proíbe um determinado usuário de mandar mensagens|Somente administrador|
|`/unmute <Apelido>`|Restaura a permissão de um determinado usuário de mandar mensagens|Somente administrador|
|`/whois <Apelido>`|Visualiza informações (incluindo o IP) de determinado usuário|Somente administrador|

Em nossa implementação, o comando `connect` é executado automaticamente por parte do cliente. Além disso, o comando `nickname` é mandatório e deve ser o primeiro utilizado após estabelecimento da conexão. Seguido dele, deve ser utilizado o comando `user` para dar informações sobre o cliente que está se conectando. Após isso, o usuário terá acesso ao restante dos comandos.
