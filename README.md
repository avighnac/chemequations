# chemequations

## Compliation

This project uses CMake. To compile:

```bash
cmake -S. -B build
cd build
make
```

## Running

In the root project directory, create a `.env` file containing the following:

| Variable | Description |
|-|-|
| FRONTEND_URL | The URL where the front-end will be running (needed for CORS). Most likely http://localhost:6942. |
| SRC_PATH | The local path to the `/src` directory **with the trailing /** (to serve HTML/JS/CSS, etc) | 

Then navigate to `build/src` and run `test` with `./test`.
