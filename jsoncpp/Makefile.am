platform=linux-gcc
dynamic_target=$(CURDIR)/libs/$(platform)-*/libjson_$(platform)-*_libmt.so
static_target=$(CURDIR)/libs/$(platform)-*/libjson_$(platform)-*_libmt.a

all-am:
	scons platform=$(platform)
	if [ -f $(dynamic_target) ];then ln -sf  $(dynamic_target) $(top_srcdir)/_lib/libjson.so; fi
	if [ -f $(static_target) ];then ln -sf $(static_target) $(top_srcdir)/_lib/libjson.a; fi

clean-am:
	scons -c platform=$(platform)
