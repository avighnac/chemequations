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
| DATABASE_PATH | The path to an Sqlite3 database (currently the code does not use this) |
| FRONTEND_URL | The URL where the front-end will be running (needed for CORS). Most likely http://localhost:6942. |

Then navigate to `build/src` and run `test` with `./test`.
