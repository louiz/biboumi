srpm:
	cmake .
	make srpm
	mkdir -p $(outdir)
	mv rpmbuild/SRPMS/*.src.rpm $(outdir)
