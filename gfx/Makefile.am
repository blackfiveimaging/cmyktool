AM_CXXFLAGS = -Wall $(GTK2_CFLAGS)

BUILT_SOURCES = chain1.cpp chain2.cpp

chain1.cpp:
	gdk-pixbuf-csource --name=chain1_data >chain1.cpp Chain1.png

chain2.cpp:
	gdk-pixbuf-csource --name=chain2_data >chain2.cpp Chain2.png

EXTRA_DIST = Chain1.png Chain2.png chain1.cpp chain2.cpp

