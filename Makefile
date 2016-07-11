BIN=Craftmine_Server
DIRECTORY=$(shell pwd)

HEADER_PATHS=-iquote Build/Classes -I /usr/local/include \
	-isystem /usr/local/include/freetype2

LIBRARIES=boost_filesystem boost_system enet ncurses

LIBRARY_FLAGS=$(addprefix -l,$(LIBRARIES))
FRAMEWORK_FLAGS=$(addprefix -framework ,$(FRAMEWORKS))
COMPILER_FLAGS=everything no-c++98-compat no-c++98-compat-pedantic no-float-equal \
	no-global-constructors no-exit-time-destructors no-newline-eof no-missing-prototypes \
	no-padded no-missing-braces no-old-style-cast

OBJECTS_FOLDER=Build/Data/Objects
APP_CONTENTS=Build/$(BIN).app/Contents

CPP_FLAGS=-arch x86_64 -std=gnu++14 -F Build
DEBUG_FLAGS=-O0 -g
RELEASE_FLAGS=-O3

LINKER_FLAGS=-arch x86_64 -o Build/$(BIN) -L Build -F Build \
	-Xlinker -rpath -Xlinker -no_deduplicate -Xlinker -dependency_info

CPP_FILES=$(wildcard Classes/*.cpp)
OBJ_FILES=$(patsubst Classes/%.cpp,$(OBJECTS_FOLDER)/%.o,$(CPP_FILES))

.SILENT: all clean

all: $(CPP_FILES) $(OBJ_FILES)
	$(info Creating object file list...)
	:> Build/Data/$(BIN).LinkFileList
	for OBJ in $(OBJ_FILES); do echo $(DIRECTORY)/$$OBJ >> Build/Data/$(BIN).LinkFileList; done
	\
	$(info Linking executable...)
	clang++ $(LINKER_FLAGS) -Xlinker $(OBJECTS_FOLDER)/$(BIN)_dependency_info.dat \
		-filelist Build/Data/$(BIN).LinkFileList -L /usr/local/lib $(LIBRARY_FLAGS) \
		$(FRAMEWORK_FLAGS)
	\
	$(info Done!)

$(OBJECTS_FOLDER)/%.o: Classes/%.cpp
	$(info Compiling $<...)
	@clang++ $(addprefix -W,$(COMPILER_FLAGS)) $(CPP_FLAGS) $(HEADER_PATHS) \
		$(DEBUG_FLAGS) -c $< -o $@

clean:
	$(info Removing $(BIN)...)
	rm -f Build/$(BIN)
	\
	$(info Removing build data...)
	rm -f $(OBJECTS_FOLDER)/*
	\
	$(info Done!)