# Junta las bibliotecas de Python, las de ffmpeg y yt-dlp en un .tar.gz (el tar conserva los
# enlaces simbólicos de los zip). Deja afuera lo que la app nunca usa, para que ocupe menos:
#   - bibliotecas que no necesita ningún programa (se ve en el ELF qué pide cada uno, a partir
#     de los ejecutables, los módulos de Python y los de OpenSSL)
#   - bibliotecas estáticas, cabeceras y las partes de Python para pruebas o desarrollo
# Uso: armar-entorno.py python.zip ffmpeg.zip yt-dlp salida EJECUTABLE...
#   (EJECUTABLE: libpython.so, libffmpeg.so, libffprobe.so: los que la app corre)
import io
import posixpath
import stat
import struct
import sys
import tarfile
import zipfile

python_zip, ffmpeg_zip, yt_dlp, salida = sys.argv[1:5]
ejecutables = sys.argv[5:]

# Se buscan en este orden, como el LD_LIBRARY_PATH que arma Herramientas::ambiente()
CARPETAS_BIBLIOTECAS = ("python/usr/lib", "ffmpeg/usr/lib")
PYTHON_LIB = "python/usr/lib/python3.12/"

# Partes de Python que yt-dlp no usa (pruebas, desarrollo, interfaz gráfica)
SOBRAN_PYTHON = ("test/", "idlelib/", "tkinter/", "turtledemo/", "lib2to3/", "ensurepip/", "config-3.12")
SOBRAN_MODULOS = ("_test", "_ctypes_test", "xxlimited", "xxsubtype", "_xxtestfuzz")


def necesita(datos):
    """Las bibliotecas (DT_NEEDED) que pide un ELF de 64 bits."""
    if datos[:4] != b"\x7fELF" or datos[4] != 2:
        return []
    shoff, = struct.unpack_from("<Q", datos, 0x28)
    shentsize, shnum = struct.unpack_from("<HH", datos, 0x3A)
    secciones = [struct.unpack_from("<IIQQQQIIQQ", datos, shoff + i * shentsize) for i in range(shnum)]
    nombres = []
    for _, tipo, _, _, offset, tamano, enlace, _, _, _ in secciones:
        if tipo != 6:    # SHT_DYNAMIC
            continue
        cadenas = secciones[enlace][4]
        for j in range(0, tamano, 16):
            etiqueta, valor = struct.unpack_from("<qQ", datos, offset + j)
            if etiqueta == 0:
                break
            if etiqueta == 1:    # DT_NEEDED
                fin = datos.index(b"\0", cadenas + valor)
                nombres.append(datos[cadenas + valor:fin].decode())
    return nombres


# Todo lo de los dos zip, con la ruta que va a tener en el tar
entradas = {}
for carpeta, ruta in (("python", python_zip), ("ffmpeg", ffmpeg_zip)):
    archivo = zipfile.ZipFile(ruta)
    for entrada in archivo.infolist():
        entradas[f"{carpeta}/{entrada.filename}".rstrip("/")] = (archivo, entrada)


def es_enlace(entrada):
    return stat.S_ISLNK(entrada.external_attr >> 16)


def real(nombre, saltos=0):
    """A qué archivo apunta, siguiendo los enlaces simbólicos (None si no está)."""
    if nombre not in entradas or saltos > 10:
        return None
    archivo, entrada = entradas[nombre]
    if not es_enlace(entrada):
        return nombre
    destino = posixpath.normpath(posixpath.join(posixpath.dirname(nombre), archivo.read(entrada).decode()))
    return real(destino, saltos + 1)


def buscar(biblioteca):
    for carpeta in CARPETAS_BIBLIOTECAS:
        encontrada = real(f"{carpeta}/{biblioteca}")
        if encontrada:
            return encontrada
    return None


def sobra_por_nombre(nombre):
    if nombre.endswith((".a", ".h")) or "/include/" in nombre or "/pkgconfig/" in nombre or "/quickjs/" in nombre:
        return True
    if nombre.startswith(PYTHON_LIB):
        resto = nombre[len(PYTHON_LIB):]
        if resto.startswith(SOBRAN_PYTHON) or "/__pycache__/" in nombre:
            return True
        if resto.startswith("lib-dynload/") and posixpath.basename(resto).startswith(SOBRAN_MODULOS):
            return True
    return False


# Las bibliotecas que se usan: lo que piden los ejecutables y los módulos que se cargan con
# dlopen (los de Python y los de OpenSSL), y lo que piden esas, y así
pendientes = []
for ruta in ejecutables:
    with open(ruta, "rb") as f:
        pendientes += [buscar(n) for n in necesita(f.read())]
for nombre, (archivo, entrada) in entradas.items():
    cargado_aparte = nombre.startswith(PYTHON_LIB) or "/ossl-modules/" in nombre or "/engines-3/" in nombre
    if cargado_aparte and ".so" in nombre and not es_enlace(entrada) and not sobra_por_nombre(nombre):
        pendientes.append(nombre)
usadas = set()
while pendientes:
    nombre = pendientes.pop()
    if not nombre or nombre in usadas:
        continue
    usadas.add(nombre)
    archivo, entrada = entradas[nombre]
    pendientes += [buscar(n) for n in necesita(archivo.read(entrada))]


def sobra(nombre):
    if sobra_por_nombre(nombre):
        return True
    destino = real(nombre)
    if destino is None:    # enlace roto (o a algo que se sacó)
        return es_enlace(entradas[nombre][1])
    base = posixpath.basename(destino)
    es_biblioteca = posixpath.dirname(destino).startswith(CARPETAS_BIBLIOTECAS) and ".so" in base
    return es_biblioteca and not destino.startswith(PYTHON_LIB) and destino not in usadas


sacado = 0
with tarfile.open(salida, "w:gz", compresslevel=9) as tar:
    for nombre, (archivo, entrada) in entradas.items():
        if not entrada.is_dir() and sobra(nombre):
            sacado += entrada.file_size
            continue
        modo = entrada.external_attr >> 16
        info = tarfile.TarInfo(nombre)
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
print(f"armar-entorno: {sacado / 1e6:.1f} MB que no hacen falta quedaron afuera")
