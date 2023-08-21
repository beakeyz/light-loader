section .rodata

inv_idt:
  dq 0, 0

section .text

[bits 64]

extern g_gdtr

global multiboot2_boot_entry
multiboot2_boot_entry:
  cld
  cli

  ; Make sure this ends up where we want it when this gets popped off
  sub rsp, 4
  mov dword [rsp], r9d
  sub rsp, 4
  mov dword [rsp], r8d
  sub rsp, 4
  mov dword [rsp], ecx
  sub rsp, 4
  mov dword [rsp], edx
  sub rsp, 4
  mov dword [rsp], esi
  sub rsp, 4
  mov dword [rsp], edi

  lgdt [rel g_gdtr]
  lidt [rel inv_idt]

  lea rbx, [rel .rel_cs]

  push 0x28
  push rbx
  retfq

.rel_cs:
  mov eax, 0x30
  mov ds, eax
  mov es, eax
  mov fs, eax
  mov gs, eax
  mov ss, eax

  lea rbx, [rel .prot_mode]

  push 0x18
  push rbx
  retfq

[bits 32]
.prot_mode:

  mov eax, 0x20
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  ; FIXME: do we need to reset special registers?

  pop edi
  pop esi
  pop edx
  pop ecx
  pop ebx
  pop eax

  ; Desired layout: 
  ; eax multiboot magic
  ; ebx: multiboot addr
  ; ecx: bootstub
  ; edx: Range count
  ; esi: Ranges
  ; edi: entry pointer

  ; Jump to our bootstub that we got passed through ecx
  jmp ecx

.spin:
  hlt
  jmp .spin

section .data

[bits 32]

global multiboot2_bootstub
multiboot2_bootstub:
  ; let's prevent relocation fuckery
  jmp .tramp
  times 4-($-multiboot2_bootstub) db 0

.tramp:

  mov esp, ecx
  add esp, .stub_stack_top - multiboot2_bootstub
  
  ; We save the values 
  push edi
  push esi
  push edx
  push ecx
  push ebx
  push eax

  ; Prepare the values for rep movsb
  mov eax, esi

  ; relocate ranges
  ; ESI + 0 = current
  ; ESI + 8 = target
  ; ESI + 16 = size
  ; ESI + 24 = next
  .reloc_loop:

    ; move values into place and start copying bytes
    mov esi, [eax]
    mov edi, [eax+8]
    mov ecx, [eax+16]
    rep movsb

    ; move to the next range
    mov eax, [eax+24]

    ; while eax is non-zero, we haven't reached the end
    test eax, eax
    jnz .reloc_loop

  .reloc_done:

    ; make sure we have entry_ptr at the top of the stack
    pop eax
    pop ebx
    pop ecx
    pop edx
    pop esi

    ; Leave last frame on the stack for us to return to

    ; eax and ebx should contain multiboot information for the kernel
    ; xor eax, eax
    ; xor ebx, ebx

    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    xor ebp, ebp

    ret

.stub_stack_bottom:
  times 16 dq 0
.stub_stack_top:

global multiboot2_bootstub_end
multiboot2_bootstub_end:
