AM_CXXFLAGS = -Wall $(GTK2_CFLAGS)

BUILT_SOURCES = chain1.cpp chain2.cpp grey.cpp rgb.cpp cmyk.cpp profile.cpp sigma.cpp hourglass.cpp logo.cpp

logo.cpp:
	gdk-pixbuf-csource --name=logo_data >logo.cpp logo.png

hourglass.cpp:
	gdk-pixbuf-csource --name=hourglass_data >hourglass.cpp hourglass.png

profile.cpp:
	gdk-pixbuf-csource --name=profile_data >profile.cpp profile.png

cmyk.cpp:
	gdk-pixbuf-csource --name=cmyk_data >cmyk.cpp cmyk.png

rgb.cpp:
	gdk-pixbuf-csource --name=rgb_data >rgb.cpp rgb.png

grey.cpp:
	gdk-pixbuf-csource --name=grey_data >grey.cpp grey.png

chain1.cpp:
	gdk-pixbuf-csource --name=chain1_data >chain1.cpp Chain1.png

chain2.cpp:
	gdk-pixbuf-csource --name=chain2_data >chain2.cpp Chain2.png

sigma.cpp:
	gdk-pixbuf-csource --name=sigma_data >sigma.cpp sigma.png

EXTRA_DIST = Chain1.png Chain2.png grey.png rgb.png cmyk.png profile.png sigma.png hourglass.png logo.png \
			 chain1.cpp chain2.cpp grey.cpp rgb.cpp cmyk.cpp profile.cpp sigma.cpp hourglass.cpp logo.cpp

