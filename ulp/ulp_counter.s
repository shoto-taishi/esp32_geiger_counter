    .global entry
.text
.bss
entry.previous_state:
    .space 4
.text
.bss
entry.current_state:
    .space 4
.text
entry:
	move r2,1
	move r3,entry.previous_state
	st r2,r3,0
	move r2,tick_count
	move r1,0
	st r1,r2,0
	jump L.3
L.2:
	reg_rd 265,18,18
	move r2,r0
	move r3,entry.current_state
	st r2,r3,0
	move r2,entry.previous_state
	ld r2,r2,0
	move r1,1
	sub r2,r2,r1 #{ if r2!=r1 goto L.5 
	jump 1f, eq
	add r2,r2,r1
	jump L.5
1:
	add r2,r2,r1 #}
	move r2,entry.current_state
	ld r2,r2,0
	move r2,r2 #{ if r2 goto L.5 
	jump 1f, eq
	jump L.5
1:           #}
	move r2,tick_count
	ld r1,r2,0
	add r1,r1,1
	st r1,r2,0
L.5:
	move r2,entry.current_state
	ld r2,r2,0
	move r3,entry.previous_state
	st r2,r3,0
L.3:
	jump L.2
L.1:

.bss
    .global tick_count
tick_count:
    .space 4
.text
halt
