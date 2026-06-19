# Wii Docker Build

Use the local Docker image when you need to build the Wii host directly.

```bash
docker build -t helengine-wii .
docker run --rm -v "$PWD":/workspace -w /workspace helengine-wii make
```

The build emits `build/helengine_wii.dol`.
