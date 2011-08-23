.SUFFIXES: .c .obj

OBJS = $(PROJ_NAME).obj
TARGET = $(PROJ_NAME).exe

.cpp.obj:
	@$(CC) /c $(CC_OPT) $< /Fo$@

.obj.exe:
	@$(LNK) $< /subsystem:console /out:$@ kernel32.lib user32.lib advapi32.lib $(EXT_LIB)

all: $(TARGET)

$(PROJ_NAME).obj: $(PROJ_NAME).cpp $(OPT_FILE)

$(TARGET): $(OBJS)

clean:
	@if exist *.obj del *.obj
	@if exist *.lst del *.lst
	@if exist $(PROJ_NAME).exe del $(PROJ_NAME).exe
