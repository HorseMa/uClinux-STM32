
TARG1 = bntest
TARG2 = bnspeed
TARG3 = exptest
TARG4 = expspeed
TARG5 = dhtest
TARG6 = rsa_test

GCC = gcc
CFLAGS = -g -ggdb

vpath %.c ../
vpath %.c ../bn
vpath %.c ../dh
vpath %.c ../rsa

SRCS = \
	bn_print.c \
	bn_lib.c \
	bn_word.c \
	bn_mul.c \
	bn_sqr.c \
	bn_add.c \
	bn_asm.c \
	bn_ctx.c \
	bn_shift.c \
	bn_div.c \
	bn_rand.c \
	bn_exp.c \
	bn_exp2.c \
	bn_mont.c \
	bn_gcd.c \
	bn_recp.c \
	bn_blind.c \
	bn_mpi.c \
	bn_prime.c \
	dh_gen.c \
	dh_check.c \
	dh_key.c \
	dh_lib.c \
	rsa_eay.c \
	rsa_lib.c \
	rsa_pk1.c \
	rsa_ssl.c \
	rsa_none.c \
	rsa_gen.c \
	rtl_cryptGlue.c \


OBJS = ${SRCS:.c=.o}

.PHONY: all clean

%.o: %.c
	$(GCC) $(CFLAGS) -o $@ -c $?

all: $(TARG1) $(TARG2) $(TARG3) $(TARG4) $(TARG5) $(TARG6)

$(TARG1): $(TARG1).c $(OBJS) 
	$(GCC) $(CFLAGS) -o $@ $^ 

$(TARG2): $(TARG2).c $(OBJS) 
	$(GCC) $(CFLAGS) -o $@ $^ 

$(TARG3): $(TARG3).c $(OBJS) 
	$(GCC) $(CFLAGS) -o $@ $^ 

$(TARG4): $(TARG4).c $(OBJS) 
	$(GCC) $(CFLAGS) -o $@ $^ 

$(TARG5): $(TARG5).c $(OBJS)
	$(GCC) $(CFLAGS) -o $@ $^

$(TARG6): $(TARG6).c $(OBJS)
	$(GCC) $(CFLAGS) -o $@ $^

clean:
	rm *.o $(TARG1) $(TARG2) $(TARG3) $(TARG4) $(TARG5) $(TARG6)
	

