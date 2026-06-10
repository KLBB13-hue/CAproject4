.section .text
.globl draw
.type draw, @function
draw:
	li a0,120
	li a1,15
	li a2,8
	li a3,65535
	addi sp,sp,-4
	sw ra,0(sp)
	jal LCD_DrawCircle
	lw ra,0(sp)
	addi sp,sp,4
	ret

.equ RANK_SIZE, 5

.globl rank_get_count
.type rank_get_count, @function
rank_get_count:
	la t0,rank_count
	lw a0,0(t0)
	ret

.globl rank_get_score
.type rank_get_score, @function
rank_get_score:
	li t0,0
	blt a0,t0,rank_get_score_zero
	li t1,RANK_SIZE
	bge a0,t1,rank_get_score_zero
	la t2,rank_scores
	slli t3,a0,2
	add t2,t2,t3
	lw a0,0(t2)
	ret

rank_get_score_zero:
	li a0,0
	ret

.globl rank_add_score
.type rank_add_score, @function
rank_add_score:
	bge zero,a0,rank_add_done
	la t0,rank_count
	lw t1,0(t0)
	li t2,RANK_SIZE
	blt t1,t2,rank_append

	la t3,rank_scores
	li t2,4
	slli t4,t2,2
	add t5,t3,t4
	lw t6,0(t5)
	bge t6,a0,rank_add_done
	sw a0,0(t5)
	j rank_bubble

rank_append:
	la t3,rank_scores
	slli t4,t1,2
	add t5,t3,t4
	sw a0,0(t5)
	addi t1,t1,1
	sw t1,0(t0)
	addi t2,t1,-1

rank_bubble:
	beqz t2,rank_add_done
	la t3,rank_scores
	slli t4,t2,2
	add t5,t3,t4
	lw t6,0(t5)
	lw t0,-4(t5)
	bge t0,t6,rank_add_done
	sw t6,-4(t5)
	sw t0,0(t5)
	addi t2,t2,-1
	j rank_bubble

rank_add_done:
	ret

.section .data
.align 2
rank_count:
	.word 0
rank_scores:
	.word 0,0,0,0,0
