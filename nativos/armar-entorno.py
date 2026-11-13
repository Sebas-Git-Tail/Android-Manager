# Junta las bibliotecas de Python, las de ffmpeg y yt-dlp en un .tar.gz (el tar conserva los
# enlaces simbólicos de los zip). Uso: armar-entorno.py python.zip ffmpeg.zip yt-dlp salida
import io
import stat
import sys
import tarfile
import zipfile

python_zip, ffmpeg_zip, yt_dlp, salida = sys.argv[1:5]

with tarfile.open(salida, "w:gz", compresslevel=9) as tar:
    for carpeta, ruta in (("python", python_zip), ("ffmpeg", ffmpeg_zip)):
        with zipfile.ZipFile(ruta) as archivo:
            for entrada in archivo.infolist():
                modo = entrada.external_attr >> 16
                info = tarfile.TarInfo(f"{carpeta}/{entrada.filename}")
                if entrada.is_dir():
                    info.type, info.mode = tarfile.DIRTYPE, 0o755
                    tar.addfile(info)
                elif stat.S_ISLNK(modo):
                    info.type, info.mode = tarfile.SYMTYPE, 0o777
                    info.linkname = archivo.read(entrada).decode()
                    tar.addfile(info)
                else:
                    datos = archivo.read(entrada)
                    info.size, info.mode = len(datos), 0o755
                    tar.addfile(info, io.BytesIO(datos))
    tar.add(yt_dlp, arcname="yt-dlp")
