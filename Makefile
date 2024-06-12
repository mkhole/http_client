TARGET := http_client
OBJS := http_client.o
LIBS := -lcurl

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

all: $(TARGET)

clean:
	rm -rf $(TARGET) *.o
