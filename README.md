# c-nde

> Winamp Media Library / Nullsoft Database Engine (NDE) reader

Use this library to read Winamp's Media Library database and convert it into JSON format.

NOTE: This is a port of my Node.js library, [node-nde](https://github.com/Wiiseguy/node-nde). This library is still in early development, and may not be fully functional yet.

## Usage

```c
#include "nde.c"

int main(void)
{
    FILE *indexFile = fopen("path/to/main.idx", "rb");
    if (!indexFile)
    {
        return 1;
    }

    FILE *dataFile = fopen("path/to/main.dat", "rb");
    if (!dataFile)
    {
        return 1;
    }

    parse(indexFile, dataFile, "main.json");

    fclose(indexFile);
    fclose(dataFile);

    return 0;
}
```

> Note: main.dat and main.idx can be found in Winamp's Plugins\ml folder.

Example of the result of `main.json`:

```js
[
  {
    filename: 'C:\\music\\song.mp3',
    title: 'Title',
    artist: 'Artist',
    year: 1986,
    genre: 'Genre',
    comment: '',
    length: 180, // length in sec
    type: 0,
    lastupd: '2016-01-04T21:47:39.000Z',
    lastplay: '2016-01-14T21:34:09.000Z',
    rating: 3,
    playcount: 32,
    filetime: '2010-11-21T14:37:32.000Z',
    filesize: 0,
    bitrate: 256,
    dateadded: '2016-01-04T21:47:39.000Z'
  },
  ...
]
```
