# Configuration and Study of a Network

## Índice:
[exp1](#exp-1)<br>
[exp2](#exp-2)<br>
[exp3](#exp-3)<br>
[exp4](#exp-4)<br>
[exp5](#exp-5)<br>
[exp6](#exp-6)<br>


## Exp 1 - Configure an IP Network

### Passos:
1. Conectar a eth 1 do TUX53 e TUX54 ao switch (Caixa branca)
2. Configurar os IP para a interface eth1 em cada um dos pc's
```shell
$ ifconfig eth1 up                    # Ativar a interface
$ ifconfig eth1 172.16.100.1/24       # Para o TUX53
$ ifconfig eth1 172.16.100.254/24     # Para o TUX54
```
3. Usar o ifconfig para ver os valores (IP e MAC)
4. Para verificar a conectividade entre os 2 computadores, foi usado `ping`(envia pacotes e espera por resposta):
```shell
$ ping 172.16.100.254 -c 6    # Para o TUX53, a flag apenas limita o nº de pacotes enviados
$ ping 172.16.100.1 -c 6      # Para o TUX54
```
5. Para inspecionar a Forwarding Table e a ARP Table (TUX53):
```shell
$ route -n      # Forwarding
$ arp -a        # ARP (ARP traduz IP no endereço MAC, deve aparecer: ? (172.16.100.1) at <MAC> on ...)
```
6. Para dar delete à entrada da tabela ARP (TUX53):
```shell
$ arp -d 172.16.100.254   # Apagar as entradas associadas ao TUX54
```
7. Iniciar o wireshark no TUX53 eth1
8. No TUX53 pingar o TUX54:
```shell
$ ping 172.16.100.254 -c 6    # É suposto observar 6 packets
```
9. É suposto existirem packets que usam o protocolo:
    - ARP -> 2 pacotes, o 1º da source a pedir a MAC para um dado IP e o 2º pacote é a resposta do destino a enviar o seu MAC para a origem
    - ICMP -> Troca de informações e erros

## Exp 2 - Implement two bridges in a switch

### Passos:
1. Configurar eth1 do TUX52:
```shell
$ ifconfig eth1 up
$ ifconfig eth1 172.16.101.1/24
```
2. Criar 2 bridges no switch (TUX52):
    - 1º limpar a config do swicth
    ```shell
    # Abrir o GKTerm e colocar a baudrate a 115200.
    > admin
    > (password é vazia)
    > /system reset-configuration
    > y
    ```
    - 2º criar as 2 bridges
    ```shell
    > /interface bridge add name=bridge100
    > /interface bridge add name=bridge101
    ```
    - 3º eliminar as portas onde os TUX's estão ligados (Assumindo que os TUX52, TUX53 e TUX54 estão ligados às interfaces ether1, ether2 e ether3 do swicth, respetivamente)
    ```shell
    > /interface bridge port remove [find interface =ether2]
    > /interface bridge port remove [find interface =ether3]
    > /interface bridge port remove [find interface =ether4]
    ```
    - 4º adicionar as portas (para a bridge100, ligar o TUX3 e TUX4, já para a bridge101, ligar apenas o TUX2)
    ```shell
    > /interface bridge port add bridge=bridge100 interface=ether3
    > /interface bridge port add bridge=bridge100 interface=ether4 
    > /interface bridge port add bridge=bridge101 interface=ether2
    ```
    - 5º para garantir que está direito, correr
    ```shell
    > /interface bridge port print
    ```

3. Começar a captura no TUX53
4. No TUX53, pingar o TUX54 e TUX52
```shell
$ ping 172.16.100.254   # Pingar o TUX54 -> vai funcionar (Existe uma bridge entre o TUX53 e TUX54)
$ ping 172.16.101.1     # Pingar o TUX52 -> "connect: Network is unreachable"
```
5. Parar as capturas e começar a capturar em todos os TUX.
6. No TUX53 fazer um broadcast
```shell
$ ping -b 172.16.100.255    # TUX54 vai receber o broadcast, mas o TUX52 não (mesma razão de cima)
```
7. Repetir o 5 e 6, mas desta vez no TUX52 e broadcats para a subnet ping -b 172.16.101.0/24
```shell
$ ping -b 172.16.101.255    # Nem o TUX54 nem o TUX53 vão receber o broadcast (mesma razão de cima)
```

## Exp 3 - Configure a Router in Linux

### Passos:
1. Ativar o eth2 no TUX54
```shell
$ ifconfig eth2 up
$ ifconfig eth2 172.16.101.253/24
```
2. Adicionar o TUX54 à bridge 101 (é preciso ligar ao switch como feito anteriormente):
```shell
> /interface bridge port remove [find interface=ether5]
> /interface bridge port add bridge=bridge101 interface=ether5
```
3. Ativar o IP forwarding e desativar o ICMP echo-ignore-broadcast:
```shell
# 1. Ativar o IP forwarding. Permite ao computador funcionar como router, encaminhando pacotes para outras subredes
$ echo 1 > /proc/sys/net/ipv4/ip_forward
# 2. Desativar ICMP echo-ignore-broadcast. Permite ao PC responder a mensagens de broadcast
$ echo 0 > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts
```
4. Observar o IP e MAC para o eth1 e eth2 no TUX54
```shell
$ ifconfig      # É suposto o endereço IP ser o correspondente a cada subnet e os MAC's serem diferentes
```
5. Adicionar rotas entre o TUX52 e TUX53 para que estejam conectados (As rotas passam pelo TUX54 que está a fazer de router)
```shell
$ route add -net 172.16.100.0/24 gw 172.16.101.253  # Para o TUX52 (<subnet destino> gw <IP do TUX>)
$ route add -net 172.16.101.0/24 gw 172.16.100.254  # Para o TUX53
# Lê-se: para chegar até à subrede é preciso ir pela gateway (gw).
```
6. Observar as rotas nos 3 TUX
```shell
$ route -n
```
7. Começar a capturar no TUX53 e pingar as outras redes
```shell
$ ping 172.16.100.254   # Tem de funcionar
$ ping 172.16.101.253   # Tem de funcionar
$ ping 172.16.101.1     # Tem de funcionar
```
8. Para de capturar no TUX53 e começar a capturar no TUX54 tanto para a eth1 como para a eth2
9. Limpar as entradas da tabela ARP nos TUX
```shell
$ arp -d 172.16.101.253 #Tux52
$ arp -d 172.16.100.254 #Tux53
$ arp -d 172.16.100.1   #Tux54
$ arp -d 172.16.101.1   #Tux54
```
10. No TUX53 pingar o TUX52 e observar os logs dos 2 wireshark do TUX54
```shell
$ ping 172.16.101.1
```

## Exp 4 - Configure a Commercial Router and Implement NAT

### Passos:
1. Conectar a ether1 do router à régua 101.12
2. Conectar a ether2 do router ao switch e adicionar à bridge101
```shell
> /interface bridge port remove [find interface=ether6]
> /interface bridge port add bridge=bridge101 interface=ether6
```
3. Trocar o cabo do switch para o router????
4. Conectar ao router, no TUX52, usando o GKTerm
```shell
# Serial Port: /dev/ttyS0
# Baudrate: 115200
# Username: admin
# Password: <ENTER>
```
5. Dar reset às configs
```shell
> /system reset-configuration
```
6. Configurar o IP do router
```shell
> /ip address add address=172.16.1.101/24 interface=ether1  # Conectar a eth1 à rede de RCOM 
> /ip address add address=172.16.101.254/24 interface=ether2 # Conectar à bridge101
```
7. Adicionar as rotas defaults no router e TUX's
```shell
> /ip route add dst-address=172.16.100.0/24 gateway=172.16.101.253  # Router console (Packets para a subrede 172.16.100 devem ser direcionados para o TUX54)
# Por algum motivo, o comando em baixo não foi usado nas fotos 
> /ip route add dst-address=0.0.0.0/0 gateway=172.16.1.101         # Router console (Associar os endereços 0.0.0.0 à porta que liga ao FTP Server)
## NÃO USAR DEFAULTS = 172.16.1.0/24
$ route add -net 172.16.1.0/24 gw 172.16.101.254 # Tux52 (Ligar ao router)
$ route add -net 172.16.1.0/24 gw 172.16.100.254 # Tux53 (Ligar ao TUX4, que por sua vez vai ter a default ligada ao router)
$ route add -net 172.16.1.0/24 gw 172.16.101.254 # Tux54 (Ligar ao router)
```
8. Verifcar se o TUX53 consegue pingar os outros TUX e o router (Deve funcionar)
```shell
$ ping 172.16.100.254 # Ping TUX54
$ ping 172.16.101.1   # Ping TUX52
$ ping 172.16.101.254 # Ping Router
```
9. No TUX52:
    - Desativar o accept_redirects
    ```shell
    $ echo 0 > /proc/sys/net/ipv4/conf/eth1/accept_redirects
    $ echo 0 > /proc/sys/net/ipv4/conf/all/accept_redirects
    ```
    - Remover a rota para a rede 172.16.100.0/24 que passa por TUX54
    ```shell
    $ route del -net 172.16.100.0 gw 172.16.101.253 netmask 255.255.255.0
    ```
    - Pingar o TUX53 e observar o caminho seguido pelos packets com o wireshark (vai ser usado o router)
    ```shell
    $ ping 172.16.100.1 # Usa a default gateway do TUX52 (leva ao router) -> Router encaminha até ao TUX54 -> TUX54 encaminha até ao TUX53
    ```
    - Fazer traceroute para o TUX53
    ```shell
    $ traceroute -n 172.16.100.1
    ```
    - Adicionar de novo a rota do TUX52 para o TUX54 e fazer o traceroute
    ```shell
    $ route add -net 172.16.100.0/24 gw 172.16.101.253
    $ traceroute -n 172.16.100.1
    ```
    - Ativar os accept redirect
    ```shell
    $ echo 1 > /proc/sys/net/ipv4/conf/eth0/accept_redirects # Não devia ser eth1???
    $ echo 1 > /proc/sys/net/ipv4/conf/all/accept_redirects
    ```
10. No TUX3, pingar o FTP Server (é suposto funcionar)
```shell
$ ping 172.16.1.10
```
11. Desativar a NAT no router
```shell
> /ip firewall nat disable 0
```
12. No TUX54 pingar o Server (172.16.1.10) e ver o que acontece (Não funciona pois os packets para redes 172.16.0.0/16 são interpretados como locais e os routers iam dropá-los)
```shell
$ ping 172.16.1.10
```
13. Reativar o NAT
```shell
> /ip firewall nat enable 0
```

## Exp 5 - DNS

### Passos:
1. Ligar o DNS em todos os TUX
```shell
$ echo "nameserver 10.227.20.3" > /etc/resolv.conf
```
2. Pingar um site usando o hostename e ver os packets no wireshark (TUX52)
```shell
$ ping kahoot.com
```

## Exp 6 - TCP connections

### Passos:
1. Começar a capturar no TUX53 e correr o programa
2. Repetir o que foi feito em 1, mas começar a transferir no TUX52 ao mesmo tempo e ver no wireshark
