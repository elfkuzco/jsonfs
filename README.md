# jsonfs

jsonfs is a FUSE filesystem that mounts a JSON file as a virtual directory
structure. Nested objects and arrays become directories while primitive
values (strings, numbers, double and booleans) become readable files.

The filesystem supports only read operations.

## Compiling

- In order to compile the program, you need to install:

  - libfuse2 (install with your package manager)
  - [libjansson](https://github.com/akheron/jansson)
  - A POSIX-compliant system (Linux/macOS)

- Clone the repository

  ```sh
  git clone git@github.com:elfkuzco/jsonfs.git
  ```

- Go inside the directory

  ```sh
  cd jsonfs
  ```

- Compile the program
  ```sh
  make
  ```

## Usage

    ```sh
    ./jsonfs [-o nonmepty] <path-to-json-file> <mountpoint>
    ```

## Unmount

    ```sh
    fusermount -u <mountpoint>
    ```
