AM_CXXFLAGS = -Wall $(GTK2_CFLAGS)

BUILT_SOURCES = chain1.cpp chain2.cpp rgb.cpp cmyk.cpp profile.cpp sigma.cpp

profile.cpp:
	gdk-pixbuf-csource --name=profile_data >profile.cpp profile.png

cmyk.cpp:
	gdk-pixbuf-csource --name=cmyk_data >cmyk.cpp cmyk.png

rgb.cpp:
	gdk-pixbuf-csource --name=rgb_data >rgb.cpp rgb.png

chain1.cpp:
	gdk-pixbuf-csource --name=chain1_data >chain1.cpp Chain1.png

chain2.cpp:
	gdk-pixbuf-csource --name=chain2_data >chain2.cpp Chain2.png

sigma.cpp:
	gdk-pixbuf-csource --name=sigma_data >sigma.cpp sigma.png

EXTRA_DIST = Chain1.png Chain2.png rgb.png cmyk.png profile.png sigma.png \
			 chain1.cpp chain2.cpp rgb.cpp cmyk.cpp profile.cpp sigma.cpp

