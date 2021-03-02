#!/usr/bin/env python3
#
# crunch setup
#
# date: Mar 1 2021
# Maintainer: glozanoa <glozanoa@uni.pe>


from setuptools import setup, find_packages
#from ama.core.version import get_version

VERSION = "v0.0.1"

f = open('README.md', 'r')
LONG_DESCRIPTION = f.read()
f.close()


setup(
    name='crunch_ama',
    version=VERSION,
    description='Password Analysis and Cracking Kit',
    long_description=LONG_DESCRIPTION,
    long_description_content_type='text/markdown',
    maintainer='glozanoa',
    maintainer_email='glozanoa@uni.pe',
    url='https://github.com/fpolit/crunch',
    license='GPL3',
    packages=find_packages(),
    include_package_data=True,
    package_data = {
        'crunch': ['*.c',
                  '*.h',
                  'Makefile'],
    },
)
