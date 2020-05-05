	.text

/* Switch from current_thread to next thread_thread, and make
 * next_thread the current_thread.  Use t0 as a temporary register,
 * which should be caller saved. */

	.globl thread_switch
	# a0 old	a1 new
thread_switch:
	/* YOUR CODE HERE */
	andi	t0,t0,0
	addi	t0,t0,1
	slli	t0,t0,13
	add		t0,a0,t0
	sd		sp,0(t0)
	sd		s0,8(t0)
	sd		s1,16(t0)
	sd		s2,24(t0)
	sd		s3,32(t0)
	sd		s4,40(t0)
	sd		s5,48(t0)
	sd		s6,56(t0)
	sd		s7,64(t0)
	sd		s8,72(t0)
	sd		s9,80(t0)
	sd		s10,88(t0)
	sd		s11,96(t0)

	andi	t0,t0,0
	addi	t0,t0,1
	slli	t0,t0,13
	add		t0,a1,t0
	ld		sp,0(t0)
	ld		s0,8(t0)
	ld		s1,16(t0)
	ld		s2,24(t0)
	ld		s3,32(t0)
	ld		s4,40(t0)
	ld		s5,48(t0)
	ld		s6,56(t0)
	ld		s7,64(t0)
	ld		s8,72(t0)
	ld		s9,80(t0)
	ld		s10,88(t0)
	ld		s11,96(t0)

	ret    /* return to ra */
