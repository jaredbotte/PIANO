//Assume that buffer is passed into r0 and uint32_t passed into r1.
.global fill_buffer
fill_buffer:
  PUSH {r4, lr}
  MOV r4, #6
  MOV r2, #0
for:
  CMP r2, #48
  BGE endfor
body:
  MOV r3, #1
  AND r3, r1
  MUL r3, r4
  ADD r3, r4
  STRH r3, [r0, r2]
  ADD r2, #2
  LSR r1, r1, #1
  B for
endfor:
  POP {r4, pc}
  