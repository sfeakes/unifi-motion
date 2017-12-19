
# define the C compiler to use
CC = gcc -O3

# Uncomment for extra logging
LOGGING =
#LOGGING = -D LOGGING_ENABLED
#LOGGING = -D LOGGING_ENABLED -D DEBUG_LOGGING_ENABLED

# debug of not
#$DBG = -g
$DBG =

MONGOOSE_CFLAGS = -DMG_ENABLE_HTTP=0 -D MG_DISABLE_MD5 -D MG_DISABLE_HTTP_DIGEST_AUTH -D MG_DISABLE_HTTP_KEEP_ALIVE -D MG_DISABLE_JSON_RPC
# define any compile-time flags
CFLAGS = -Wall $(DBG) $(LIBS) $(LOGGING) $(MONGOOSE_CFLAGS)

# Add inputs and outputs from these tool invocations to the build variables 

# define the C source files
SRCS = unifi-motion.c mongoose.c

OBJS = $(SRCS:.c=.o)

# define the executable file
MAIN = release/unifi-motion

all:    $(MAIN) 
  @echo: $(MAIN) have been compiled

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN) $(MAIN_U)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

install: $(MAIN)
	./release/install.sh
