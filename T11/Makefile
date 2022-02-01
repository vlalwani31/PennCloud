TARGETS = webserver

all: $(TARGETS)

%.o: %.cc
	g++ $^ -c -o $@

webserver: webserver.o process_request.o utils.o
	g++ $^ -lpthread -o $@

pack:
	rm -f submit-hw3.zip
	zip -r submit-hw3.zip README Makefile *.c* *.h*

clean::
	rm -fv $(TARGETS) *~ *.o submit-hw3.zip

realclean:: clean
	rm -fv cis505-hw3.zip
