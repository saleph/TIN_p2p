## Preparation

### Boost
1. Download [Boost C++ lib](http://www.boost.org/doc/libs/1_65_1/more/getting_started/unix-variants.html) or get from distro's repository (we have used 1.65.1 version).
2. Unzip boost_1_65_1 at home dir `~/`.
3. `$ ./bootstrap.sh`
4. `$ ./b2`

## Compilation
`p2p` target is the main application with UI. `p2pTests` are unittests.
```
$ mkdir BUILD && cd BUILD
$ cmake ..
$ make {p2p, p2pTests} 
```

