targets = server
sources = server.cpp sqlconn.cpp http.cpp
objects = server.o sqlconn.o	http.o

$(targets): $(objects)
	g++ $(objects) -o $@ -lmysqlclient -lz

%.o: %.cpp
	g++ -c $< -o $@

clean:
	rm -f $(objects) server