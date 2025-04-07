targets = server
sources = sqlconn.cpp http.cpp server.cpp
objects = sqlconn.o	http.o server.o

$(targets): $(objects)
	g++ $(objects) -o $@ -lmysqlclient -lz -lssl -lcrypto

%.o: %.cpp
	g++ -c $< -o $@

clean:
	rm -f $(objects) $(targets)