TARGET = radio-proxy
SRCPTH = src
SRC = $(notdir $(wildcard $(SRCPTH)/*.cpp))
LIBPTH = bin
OBJ = $(addprefix $(LIBPTH)/,$(SRC:.cpp=.o))
DEP = $(addprefix $(LIBPTH)/,$(SRC:.cpp=.d))
FLAGS = -Wall -O3 -std=c++11
LINK = -Wextra

all: main

main: $(OBJ)
	g++ $(FLAGS) $(OBJ) -o $(TARGET)  $(LINK)

debug: $(OBJ)
	g++ -g $(FLAGS) $(OBJ) -o $(TARGET) $(LINK)

$(LIBPTH)/%.o: $(SRCPTH)/%.cpp
	g++ -c $(FLAGS) $< -o $(LIBPTH)/$(basename $(notdir $<)).o

clean:
	rm -f $(TARGET) $(OBJ)
