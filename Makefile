.PHONY: virtualenv dist dist-upload

install: virtualenv require
	env/bin/python3 setup.py install --use-feature=in-tree-build

installdev: virtualenv requiredev
	env/bin/python3 setup.py install --verbose --use-feature=in-tree-build

virtualenv:
	python3 -m venv --prompt 'pystegseek' env
	@echo
	@echo "Virtual enviroment was created. Now activate it: source env/bin/activate"
	@echo
	env/bin/python3 -m pip install --upgrade pip

require:
	env/bin/python3 -m pip install -r requirements.txt

requiredev:
	env/bin/python3 -m pip install -r requirements-dev.txt

pkg:
	env/bin/python3 setup.py install --use-feature=in-tree-build

pkgdev:
	env/bin/python3 setup.py install --verbose --use-feature=in-tree-build

dist:
	rm -rf dist/*
	env/bin/python3 setup.py sdist
	env/bin/python3 setup.py bdist_wheel

dist-upload:
	env/bin/twine upload dist/*
