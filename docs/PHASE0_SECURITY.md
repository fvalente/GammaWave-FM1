# Firma de los paquetes .fwsc: propuesta Ed25519 (fase 0, sin implementar)

Estado: **propuesta técnica**. No hay claves, ni firma, ni verificación en el árbol. Hoy la única
garantía es el SHA-256 (integridad), que viaja en el mismo sitio que el paquete.

## 1. Qué problema resuelve y qué no

El SHA-256 obligatorio (`web/index_pkg.html`, `tools/fm1_install.py`) prueba que **los bytes son los
que se publicaron**. No prueba **quién** los publicó: si `versions.json` y el `.fwsc` se cambian
juntos en el sitio, el hash coincide con el paquete del atacante. Una firma Ed25519 con la clave
pública en un canal distinto (el código del instalador, el repositorio git) cierra ese hueco.

No resuelve:

* **El host comprometido.** La verificación ocurre en el navegador o en el CLI del usuario; si ese
  equipo está comprometido, la firma no protege. El FM-1 no verifica firmas (su loader es mínimo y
  no tiene criptografía), así que la garantía es de la cadena host-sitio, no del dispositivo.
* **Replay / rollback.** Una firma válida de una versión vieja sigue siendo válida. Hace falta un
  mínimo de versión (`FM-1_9nn` creciente) en el verificador y en el instalador.
* **Malware dentro de un firmware firmado.** Firmar no revisa el código.
* **La toolchain y el SDK** (ver §6).

## 2. Formato propuesto

Paquete y firma separados, para que un `.fwsc` viejo siga siendo válido y la firma sea un archivo
más de la publicación:

```
firmware/QuantumGammaWave-v1.4-FM-1_982.fwsc          los bytes que se instalan
firmware/QuantumGammaWave-v1.4-FM-1_982.fwsc.sig     64 bytes, Ed25519 puro (sin ASN.1)
firmware/versions.json                                 {version, pkg, sha256, size, sig, keyid, min}
```

* Se firma el **archivo completo** (los bytes del `.fwsc`), no el SHA-256 ni el payload descifrado:
  el lector del instalador (`web/fm1pkg.js`) ya rechaza un paquete manipulado por sus CRC, así que la
  firma cubre exactamente lo que se descarga.
* `.sig` = firma cruda de 64 bytes (compatible con WebCrypto `Ed25519`, libsodium, `cryptography`).
* `keyid` = los primeros 8 bytes hex del SHA-256 de la clave pública, para rotar sin ambigüedad.
* `min` = versión mínima instalable (anti-rollback): el verificador rechaza un paquete firmado cuya
  identidad `FM-1_nnn` sea menor que `min`.
* `SHA256SUMS` sigue existiendo: es lo que usan quienes instalan a mano.

## 3. Dónde vive la clave pública

1. **En el código del instalador**, no en el sitio: `web/keys.js` (una lista de `keyid` → clave de 32
   bytes) y `tools/fm1_install.py` (`TRUSTED_KEYS`). Así el atacante que controla el sitio no puede
   cambiar la clave y el paquete a la vez.
2. En el README y en `docs/`, publicadas como texto (no son secretas), con su huella.
3. Rotación: se añade la clave nueva a la lista antes de firmar con ella; las viejas se retiran con
   un `keyid` de revocación explícito. Nunca se sustituye una clave por otra en el mismo commit que
   cambia el formato de firma.

## 4. Custodia de la clave privada (requisito, no sugerencia)

* Fuera de CI y fuera del repositorio: nunca en `secrets` de GitHub Actions, nunca en un runner.
* Un solo equipo offline (o un HSM/tarjeta) con la clave; el paquete se firma **después** de que CI
  lo construya, y el workflow **no** puede firmar (solo publicar el binario y su SHA-256).
* La firma se sube a mano (o desde el equipo offline) junto al paquete ya publicado; el workflow
  verifica la firma y falla si no cuadra, pero no la produce.
* Copia de respaldo de la clave en dos lugares físicos distintos, con procedimiento escrito; si se
  pierde, hay que publicar una clave nueva en el instalador (una release que nadie puede verificar

## 5. Verificación

**Web** (`web/index_pkg.html`, junto al SHA-256 obligatorio de hoy):

```js
// 1. el SHA-256 publicado (ya existe)  2. la firma con la clave del código, no del sitio
const ok = await crypto.subtle.verify({ name: "Ed25519" }, pubKey, sig, pkg);
if (!ok) { sayK("badsig"); return; }           // nada se habilita: image queda en null
```

* `Ed25519` está en WebCrypto en Chrome/Edge/Firefox/Safari actuales; si `crypto.subtle.verify` con
  `Ed25519` no existe, se usa un verificador JS auditado y vendorizado (p. ej. `@noble/ed25519`,
  ~5 KB) — nunca una implementación escrita para la ocasión.
* La comprobación va **antes** de `image = logicalImage(pkg)`: sin firma válida el botón INSTALL no
  se habilita (mismo camino que `nohash` / `badhash`).

**CLI** (`tools/fm1_install.py`):

```python
# cryptography >= 42: Ed25519PublicKey.from_public_bytes(...).verify(sig, raw)
```

Un `<paquete>.fwsc.sig` ausente se trata como hoy un SHA-256 ausente: **error** (exit 2), no aviso.
Un escape tipo `--no-signature` no debería existir; si se añade, que exija `--force` y lo diga.

**Mensajes** (los tres idiomas de `index_pkg.html`): "sin firma", "la firma no corresponde a ningún
paquete publicado", "paquete firmado por una clave retirada", "versión anterior a la mínima".

## 6. Cadena de suministro (lo que la firma del paquete no cubre)

| Eslabón | Hoy | Propuesta |
|---|---|---|
| Paquete `.fwsc` | SHA-256 obligatorio (web + CLI) | firma Ed25519 (esta propuesta) |
| SDK AC79 (3 archivos) | `SDK_SHA256` en `tools/build.py` ✔ | mantener; añadir versión del SDK al pie del paquete |
| Toolchain JieLi | descarga de `pkgman.jieliapp.com/s/linux-toolchain`, **sin hash** | hash en `tools/get_toolchain.sh` (`JIELI_TOOLCHAIN_SHA256`) y su valor revisado en CI; el script imprime el observado |
| Runner / actions | `actions/*@v4` por tag mayor | fijar por SHA completo |
| Sitio (Pages) | el workflow publica lo que construye | la firma y la clave viven en el repo, no en el sitio |

## 7. Orden de implementación (cuando se apruebe)

1. Claves: generar el par en el equipo offline; publicar la pública en `docs/` y en el repo (PR).
2. `web/fm1pkg.js`: `verifySignature(bytes, sig, keyid)` (WebCrypto + respaldo vendorizado) y tests en
   `web/test_pkg_bounds.mjs` / `web/test_web.mjs` con un par **de prueba** generado en el test (nunca
   en el repositorio).
3. `tools/fm1_install.py`: verificación obligatoria, incluida en `--verify-only`.
4. `web/make_site.py`: publicar `sig`, `keyid`, `min` y copiar el `.sig`.
5. Workflow: un paso que verifica la firma publicada (falla si no cuadra) y no puede firmar.
6. Anti-rollback (`min`) y mensajes.

Nada de esto se ha implementado en esta etapa: no se han generado claves, ni se ha añadido una
dependencia criptográfica, ni se ha tocado el camino de instalación.

  con la anterior es peor que no firmar).
