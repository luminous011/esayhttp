targets = server
sources = sqlconn.cpp http.cpp server.cpp
objects = sqlconn.o	http.o server.o

$(targets): $(objects)
	g++ $(objects) -o $@ -lmysqlclient -lz -lssl -lcrypto -g

%.o: %.cpp
	g++ -c $< -o $@ -g

clean:
	rm -f $(objects) $(targets)