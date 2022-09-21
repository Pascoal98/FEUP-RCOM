- Para esta prática vai ser preciso 3 terminais diferentes, um para usar o comando `socat` para criar 2 portas série virtuais interligadas entre si.
```bash
$ sudo socat -d  -d  PTY,link=/dev/ttyS10,mode=777   PTY,link=/dev/ttyS11,mode=777
```

- Depois compilamos os dois programas em C que o professor nos deu e depois é executar cada um com uma porta de série diferente.

```bash
$ gcc -o sender write_noncanonical.c

$ gcc -o receiver read_noncanonical.c
```

- Após compilar os programas, abrimos dois terminais e corremos como o seguinte exemplo:

```bash
$ ./sender /dev/ttyS10
```

```bash
$ ./receiver /dev/ttyS11
```