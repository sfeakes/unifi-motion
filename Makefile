
# define the C compiler to use
CC = gcc -O3

# Uncomment for extra logging
LOGGING =
#LOGGING = -D LOGGING_ENABLED
#LOGGING = -D DEBUG_LOGGING_ENABLED

# debug of not
#$DBG = -g
$DBG =

MONGOOSE_CFLAGS = -D MG_ENABLE_MQTT=1 -D MG_ENABLE_MQTT_BROKER=0 -D MG_ENABLE_HTTP=0 -D MG_ENABLE_SSL=0 -D MG_ENABLE_HTTP_WEBSOCKET0 -D MG_ENABLE_DNS_SERVER=0 -D MG_ENABLE_COAP=0 -D MG_ENABLE_BROADCAST=0 -D MG_ENABLE_GETADDRINFO=0 -D MG_ENABLE_THREADS=0 -D MG_DISABLE_HTTP_DIGEST_AUTH -D CS_DISABLE_MD5
# define any compile-time flags
CFLAGS = -Wall $(DBG) $(LIBS) $(LOGGING) $(MONGOOSE_CFLAGS)

# Add inputs and outputs from these tool invocations to the build variables 

# define the C source files
SRCS = unifi-motion.c mongoose.c config.c

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
