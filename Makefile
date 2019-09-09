CFLAGS+=-Wall -Werror -std=gnu99 -O2 
LDFLAGS+=`curl-config --libs`

TARGETS=app.o
OBJS=buf.o log.o json.o jsmn.o

all: $(TARGETS) .gitignore

clean:
	$(RM) $(TARGETS)
	$(RM) $(OBJS)

app: $(OBJS) app.o
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: all clean
