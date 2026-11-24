# DanteViewer

**Autora:** Paula Rodríguez  
## Código

| Archivo | Qué hace |
|---|---|
| `main.cpp` | Ventana Win32, contexto OpenGL, entrada, lógica, pose, curva de Bézier, cámara, luces y dibujo |
| `Gltf.h` | Cargador de glTF 2.0: mallas, jerarquía de nodos, materiales y texturas |
| `Json.h` | Lector de JSON mínimo (para leer el `.gltf`) |
| `Png.h` | Decodificador de PNG con su propio *inflate* (para las texturas) |
| `assets/dante/` | Modelo `scene.gltf`, `scene.bin` y texturas |

## Arquitectura del software

```
                    ┌──────────────────────────── main.cpp ────────────────────────────┐
 Teclado / mouse ─▶ │  WndProc (eventos)                                               │
 Panel ImGui     ─▶ │      │                                                            │
                    │      ▼                                                            │
                    │  update(dt)  ── modo LIBRE (WASD)  ┐                              │
                    │      │          modo BEZIER (A→B)  ┴▶ posición, rumbo, g_move     │
                    │      ▼                                 (0 = quieto, 1 = corriendo)│
                    │  render()                                                         │
                    │   ├─ matPerspective() + matLookAt()   → cámara                    │
                    │   ├─ luces Phong (puntual orbital + relleno)                      │
                    │   ├─ suelo y curva de Bézier                                      │
                    │   └─ matModel() → poseModel() → drawModel()  → personaje          │
                    └──────────────────────────────────────────────────────────────────┘
                                   ▲
              Gltf.h ──▶ Json.h (lee .gltf)      Png.h (inflate + PNG → texturas)
```

**Flujo por cuadro:** 1) se leen las entradas, 2) `update()` mueve al personaje y mezcla suavemente entre *quieto* y *correr*, 3) `poseModel()` deforma los vértices según esa mezcla, 4) `render()` arma las matrices de cámara y dibuja.

## Cómo cumple cada requisito

- **Matrices de traslación de la cámara:** `matLookAt()` construye la matriz de vista (rotación por traslación de `-ojo`) y `matPerspective()` la de proyección; ambas se cargan con `glLoadMatrixf`. La cámara sigue al personaje y se puede trasladar con las flechas y RePág/AvPág, o con el panel.
- **Rotación de objetos:** `matModel()` = Traslación(x, 0, z) · RotaciónY(rumbo). El personaje gira hacia donde corre.
- **Carga de GLTF:** `Gltf.h` lee `scene.gltf` y `scene.bin`, recorre los nodos aplicando sus matrices y carga las texturas PNG.
- **Iluminación (Phong):** una luz puntual naranja que orbita y una luz de relleno azul. Cada una aporta componente ambiente, difusa y especular. Se usa el pipeline fijo de OpenGL, que evalúa el modelo de Phong por vértice.
- **Bézier:** curva cúbica `B(t) = (1-t)³P0 + 3(1-t)²t P1 + 3(1-t)t² P2 + t³ P3` en el plano XZ. Se recorre a velocidad constante usando una tabla de longitud de arco. El rumbo sale de la derivada de la curva.
- **Entradas de teclado:** ver controles. Con Dear ImGui hay además un panel.

## Pose de la animación

El modelo GLTF está en pose de T y **no trae esqueleto ni animaciones**. Por eso la pose se calcula en `poseModel()`: cada vértice se mueve según su zona (brazos bajados con flexión de codo, piernas con cadera y rodilla, rebote e inclinación del cuerpo). Al quedarse quieto, esos movimientos se mezclan a cero.

## Controles

| Tecla | Acción |
|---|---|
| `W A S D` | Correr libremente (relativo a la cámara). Al soltar, queda quieto |
| `B` | Correr por la curva de Bézier de A (verde) a B (rojo) |
| `R` | Volver a A |
| `1` / `2` | Modo libre / modo Bézier |
| Flechas, `RePág`, `AvPág` | Trasladar la cámara |
| Mouse (arrastrar) y rueda | Orbitar y zoom |
| `Q` / `E` | Girar el personaje (rotación del objeto) |
| `L` | Luz orbital activada o pausada |
| `F` | Alambre / sólido |
| `M` | Modelo GLTF / modelo básico de primitivas |
| `Esc` | Salir |

## Cumplimiento del parcial

|---|---|---|
| 1 | GitHub | Sube esta carpeta a un repositorio (ver abajo) |
| 2 | README.md con nombre, código y arquitectura | Este archivo |
| 3 | Matrices de traslación de la cámara | `matLookAt()` (rotación por traslación de `-ojo`), `matPerspective()` y la traslación con flechas / RePág / AvPág |
| 3 | Rotación de objetos | `matModel()` y giro manual del personaje con `Q` / `E` |
| 3 | Carga de GLTF | `Gltf.h`, `Json.h`, `Png.h` |
| 3 | Iluminación (Phong) | `render()`: luz puntual orbital y luz de relleno con ambiente, difusa y especular |
| 3 | Bézier | `bezierPoint()`, `bezierTangent()` y `buildArc()` |
| 4 | Entradas de teclado y Dear ImGui | `update()`, `WM_KEYDOWN` y `drawUI()` |
| 5 | Video explicativo en YouTube | Enlace al inicio de este archivo |

### Subir el proyecto a GitHub

```
git init
git add .
git commit -m "Parcial: visor 3D de Dante"
git branch -M main
git remote add origin https://github.com/TU_USUARIO/TU_REPOSITORIO.git
git push -u origin main
```

## Créditos

Modelo 3D **"DMC 4 DANTE"** de [Tremolo_1404_](https://sketchfab.com/Tremolo_1404_), licencia [CC-BY 4.0](http://creativecommons.org/licenses/by/4.0/). Fuente: https://sketchfab.com/3d-models/dmc-4-dante-190b70ebb9c344928c583250df45e33a

## Limitaciones conocidas

- El cargador no soporta esqueletos, animaciones, archivos `.glb` ni buffers en base64.
- Phong se evalúa por vértice. Para Phong por píxel habría que usar shaders GLSL.
- La gabardina se deforma poco al correr, y el pantalón puede asomar por debajo.
