#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="$repo_root/build/bios"
kernel_build_dir="$repo_root/build/kernel"
stage1_source="$repo_root/product/kernel/arch/x86_64/boot/bios/stage1.asm"
stage2_source="$repo_root/product/kernel/arch/x86_64/boot/bios/stage2.asm"
kernel_source="$repo_root/product/kernel/src/kernel.c"
physmem_source="$repo_root/product/kernel/src/physmem.c"
vm_source="$repo_root/product/kernel/src/vm.c"
address_space_source="$repo_root/product/kernel/src/address_space.c"
elf_user_loader_source="$repo_root/product/kernel/src/elf_user_loader.c"
initrd_source="$repo_root/product/kernel/src/initrd.c"
process_source="$repo_root/product/kernel/src/process.c"
capability_table_source="$repo_root/product/kernel/src/capability_table.c"
ipc_endpoint_source="$repo_root/product/kernel/src/ipc_endpoint.c"
shared_memory_source="$repo_root/product/kernel/src/shared_memory.c"
virtio_pci_source="$repo_root/product/kernel/src/virtio_pci.c"
policy_token_source="$repo_root/product/kernel/src/policy_token.c"
kernel_entry_source="$repo_root/product/kernel/arch/x86_64/kernel_entry.asm"
interrupts_source="$repo_root/product/kernel/arch/x86_64/interrupts.asm"
user_source="$repo_root/product/kernel/arch/x86_64/user_program.asm"
linker_script="$repo_root/product/kernel/linker.ld"
user_linker_script="$repo_root/product/kernel/user_linker.ld"
include_dir="$repo_root/product/kernel/include"
stage1_bin="$build_dir/stage1.bin"
stage2_raw="$build_dir/stage2.raw"
stage2_bin="$build_dir/stage2.bin"
kernel_elf="$build_dir/kernel.elf"
user_elf="$build_dir/user.elf"
initrd_manifest="${INITRD_MANIFEST:-$repo_root/product/services/examples/echo.manifest.json}"
initrd_output="$build_dir/initrd.bin"
user_image_asm="$kernel_build_dir/user_image_blob.asm"
user_image_obj="$kernel_build_dir/user_image_blob.o"
output="$build_dir/stage1.img"
stage2_sectors=24
kernel_lba=$((1 + stage2_sectors))

mkdir -p "$build_dir" "$kernel_build_dir"
read -r -a kernel_extra_cflags <<< "${KERNEL_CFLAGS_EXTRA:-}"
read -r -a user_extra_nasmflags <<< "${USER_NASMFLAGS_EXTRA:-}"
gcc -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -mno-red-zone -mno-sse -mno-mmx -mno-80387 -msoft-float -mcmodel=kernel \
    "${kernel_extra_cflags[@]}" -I"$include_dir" -c "$kernel_source" -o "$kernel_build_dir/kernel.o"
gcc -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -mno-red-zone -mno-sse -mno-mmx -mno-80387 -msoft-float -mcmodel=kernel \
    "${kernel_extra_cflags[@]}" -I"$include_dir" -c "$physmem_source" -o "$kernel_build_dir/physmem.o"
gcc -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -mno-red-zone -mno-sse -mno-mmx -mno-80387 -msoft-float -mcmodel=kernel \
    "${kernel_extra_cflags[@]}" -I"$include_dir" -c "$vm_source" -o "$kernel_build_dir/vm.o"
gcc -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -mno-red-zone -mno-sse -mno-mmx -mno-80387 -msoft-float -mcmodel=kernel \
    "${kernel_extra_cflags[@]}" -I"$include_dir" -c "$address_space_source" -o "$kernel_build_dir/address_space.o"
gcc -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -mno-red-zone -mno-sse -mno-mmx -mno-80387 -msoft-float -mcmodel=kernel \
    "${kernel_extra_cflags[@]}" -I"$include_dir" -c "$elf_user_loader_source" -o "$kernel_build_dir/elf_user_loader.o"
gcc -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -mno-red-zone -mno-sse -mno-mmx -mno-80387 -msoft-float -mcmodel=kernel \
    "${kernel_extra_cflags[@]}" -I"$include_dir" -c "$initrd_source" -o "$kernel_build_dir/initrd.o"
gcc -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -mno-red-zone -mno-sse -mno-mmx -mno-80387 -msoft-float -mcmodel=kernel \
    "${kernel_extra_cflags[@]}" -I"$include_dir" -c "$process_source" -o "$kernel_build_dir/process.o"
gcc -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -mno-red-zone -mno-sse -mno-mmx -mno-80387 -msoft-float -mcmodel=kernel \
    "${kernel_extra_cflags[@]}" -I"$include_dir" -c "$capability_table_source" -o "$kernel_build_dir/capability_table.o"
gcc -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -mno-red-zone -mno-sse -mno-mmx -mno-80387 -msoft-float -mcmodel=kernel \
    "${kernel_extra_cflags[@]}" -I"$include_dir" -c "$ipc_endpoint_source" -o "$kernel_build_dir/ipc_endpoint.o"
gcc -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -mno-red-zone -mno-sse -mno-mmx -mno-80387 -msoft-float -mcmodel=kernel \
    "${kernel_extra_cflags[@]}" -I"$include_dir" -c "$shared_memory_source" -o "$kernel_build_dir/shared_memory.o"
gcc -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -mno-red-zone -mno-sse -mno-mmx -mno-80387 -msoft-float -mcmodel=kernel \
    "${kernel_extra_cflags[@]}" -I"$include_dir" -c "$virtio_pci_source" -o "$kernel_build_dir/virtio_pci.o"
gcc -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -mno-red-zone -mno-sse -mno-mmx -mno-80387 -msoft-float -mcmodel=kernel \
    "${kernel_extra_cflags[@]}" -I"$include_dir" -c "$policy_token_source" -o "$kernel_build_dir/policy_token.o"
nasm -f elf64 "$kernel_entry_source" -o "$kernel_build_dir/kernel_entry.o"
nasm -f elf64 "$interrupts_source" -o "$kernel_build_dir/interrupts.o"
nasm -f elf64 "${user_extra_nasmflags[@]}" "$user_source" -o "$kernel_build_dir/user_program.o"
ld -nostdlib -z max-page-size=0x1000 -T "$user_linker_script" \
    -o "$user_elf" "$kernel_build_dir/user_program.o"
python3 "$repo_root/product/tools/make-initrd.py" \
    "$initrd_manifest" "$user_elf" "$initrd_output" >/dev/null
printf 'bits 64\nsection .rodata\nglobal user_image_start\nglobal user_image_end\nuser_image_start:\nincbin "%s"\nuser_image_end:\nsection .note.GNU-stack noalloc noexec nowrite progbits\n' \
    "$user_elf" > "$user_image_asm"
nasm -f elf64 "$user_image_asm" -o "$user_image_obj"
ld -nostdlib -z max-page-size=0x1000 -T "$linker_script" \
    -o "$kernel_elf" "$kernel_build_dir/kernel_entry.o" "$kernel_build_dir/interrupts.o" \
    "$kernel_build_dir/kernel.o" "$kernel_build_dir/physmem.o" "$kernel_build_dir/vm.o" \
    "$kernel_build_dir/address_space.o" \
    "$kernel_build_dir/elf_user_loader.o" \
    "$kernel_build_dir/initrd.o" \
    "$kernel_build_dir/process.o" "$kernel_build_dir/capability_table.o" \
    "$kernel_build_dir/ipc_endpoint.o" "$kernel_build_dir/shared_memory.o" \
    "$kernel_build_dir/virtio_pci.o" \
    "$kernel_build_dir/policy_token.o" \
    "$kernel_build_dir/user_program.o" "$user_image_obj"
kernel_bytes="$(stat -c '%s' "$kernel_elf")"
kernel_sectors=$(( (kernel_bytes + 511) / 512 ))
if [[ "$kernel_sectors" -lt 1 || "$kernel_sectors" -gt 255 ]]; then
    echo "kernel ELF must fit in 1..255 sectors; got $kernel_sectors" >&2
    exit 1
fi
kernel_padded="$build_dir/kernel.padded.elf"
cp "$kernel_elf" "$kernel_padded"
truncate -s $((kernel_sectors * 512)) "$kernel_padded"
initrd_bytes="$(stat -c '%s' "$initrd_output")"
initrd_sectors=$(( (initrd_bytes + 511) / 512 ))
if [[ "$initrd_sectors" -lt 1 || "$initrd_sectors" -gt 255 ]]; then
    echo "initrd must fit in 1..255 sectors; got $initrd_sectors" >&2
    exit 1
fi
initrd_padded="$build_dir/initrd.padded.bin"
cp "$initrd_output" "$initrd_padded"
truncate -s $((initrd_sectors * 512)) "$initrd_padded"
initrd_lba=$((kernel_lba + kernel_sectors))

nasm -f bin -dSTAGE2_SECTORS="$stage2_sectors" -dKERNEL_LBA="$kernel_lba" \
    -dKERNEL_SECTORS="$kernel_sectors" -dKERNEL_BYTES="$kernel_bytes" \
    -dINITRD_LBA="$initrd_lba" -dINITRD_SECTORS="$initrd_sectors" \
    -dINITRD_BYTES="$initrd_bytes" \
    "$stage2_source" -o "$stage2_raw"
stage2_size="$(stat -c '%s' "$stage2_raw")"
stage2_capacity=$((stage2_sectors * 512))
if [[ "$stage2_size" -gt "$stage2_capacity" ]]; then
    echo "stage2 exceeds fixed capacity: $stage2_size > $stage2_capacity" >&2
    exit 1
fi
cp "$stage2_raw" "$stage2_bin"
truncate -s "$stage2_capacity" "$stage2_bin"

nasm -f bin -dSTAGE2_LBA=1 -dSTAGE2_SECTORS="$stage2_sectors" "$stage1_source" -o "$stage1_bin"
stage1_size="$(stat -c '%s' "$stage1_bin")"
if [[ "$stage1_size" -ne 512 ]]; then
    echo "stage1 must be exactly 512 bytes; got $stage1_size" >&2
    exit 1
fi
cat "$stage1_bin" "$stage2_bin" "$kernel_padded" "$initrd_padded" > "$output"
size="$(stat -c '%s' "$output")"
signature="$(od -An -tx1 -j 510 -N 2 "$stage1_bin" | tr -d ' \n')"
if [[ "$signature" != "55aa" ]]; then
    echo "invalid boot signature: expected 55aa, got $signature" >&2
    exit 1
fi
echo "built $output (${size} bytes: stage1=${stage1_size}, stage2=${stage2_size}/${stage2_capacity}, kernel=${kernel_bytes} bytes/${kernel_sectors} sectors, kernel_lba=${kernel_lba}, initrd=${initrd_bytes} bytes/${initrd_sectors} sectors, initrd_lba=${initrd_lba}; signature 0x55AA)"
