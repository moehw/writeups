# nasm -f bin shellcode.asm -o shellcode.bin
gcc shellcode.c -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -masm=intel -m32 -T link.ld -fno-pie -o shellcode.bin