# ProgrammablePulling
Programmable pulling experiments (based on OpenGL Insights "Programmable Vertex Pulling" article by Daniel Rakos)

# Sample image

![Sample image](http://i.imgur.com/kTYIeje.jpg)

# Explanation of parameters

## Programmability

* None: Passes per-vertex inputs using built-in vertex array object and vertex shader "in" variables
* Pull vertex: gl_VertexID comes from index buffer passed through VAO as usual. gl_VertexID is used to index the vertex data.
* Pull index and vertex: Non-indexed draw is used, and gl_VertexID is used to manually read the VertexID from the index buffer. Presumably circumvents [post-transform cache](https://www.khronos.org/opengl/wiki/Post_Transform_Cache).
* Pull with soft cache: See "Special Modes" below.
* Assembly in GS: See "Special Modes" below.
* Assembly in TS: See "Special Modes" below.

## Layout

* AoS: Position data is stored as XYZ XYZ XYZ in memory (or sometimes XYZW for padding). VAO implementation passes a vec3 as usual. texture/image/SSBO implementations either load all four XYZW in one load (the W is padding), or they do three separate 1-float loads (to avoid needing padding.)
* SoA: Position data is stored as XXX YYY ZZZ in memory. VAO implementation passes 3 separate single float attributes. texture/image/SSBO implementations do three loads of a single float, one load from each buffer.

**Note**: Some people think I'm talking about PTN PTN PTN vs. PPP TTT NNN when I say AoS/SoA, but I'm actually talking about AoS/SoA at the next lower level, between the XYZ of individual vec3s.

For specifics, the shaders used in each configuration are independent, and in the shaders/ folder of the project, so they can be consulted to see the exact syntax used.

There is also a mode that measures PTN PTN PTN with VAOs (not programmable). In this benchmark, there are only positions and normals, as used in all tests. No texcoords.

## Special Modes

Some modes of the program are a bit fancier.

### OBJ-Style multi-index

Uses fully programmable pulling to read mesh data layed out in the same format as obj. This means there is a separate index buffer for positions and for normals.

### OBJ-Style + soft cache

Implements a post-transform vertex cache in software. It was initially based on the code in "Deferred Attribute Interpolation Shading" (Listing 3.1) in GPU Pro 7 (authors: Christoph Schied and Carsten Dachsbacher.) However, it has been totally rewritten since then, but that article might be interesting to you anyways.

The cache is implemented using a hash table that maps pairs of `<position index, normal index>` to the ID of a vertex in a big linearly allocated buffer. Separate chaining is used for entries within the same bucket. Access to the hash table is guarded by a per-bucket shared readers-writer lock.

This mode can be configured using the GUI with the following options:

#### Soft vertex cache bucket bits

Decides the size of the hash table used for the cache. (= `2^n` buckets)

#### Soft vertex cache read lock attempts

Number of times the vertex shader should try to acquire the cache bucket's shared readers-writer lock **for read access** before giving up and recomputing the vertex.

#### Soft vertex cache write lock attempts

Number of times the vertex shader should try to acquire the cache bucket's shared readers-writer lock **for write access** before giving up and using the recomputed vertex without caching it for somebody else to reuse it.

#### Soft vertex cache entries per bucket

Number of entries per bucket in the hash map used for the cache.

### Assembly in GS

Runs 6 vertex shader instances per triangle, and each instance outputs either a position or a normal. The 3 positions and 3 normals are assembled together into a primitve in a geometry shader.

### Assembly in TS

Runs 6 vertex shader instances per triangle, and each instance outputs either a position or a normal. The 3 positions and 3 normals are assembled together into a primitve in a tessellation shader. *Currently bugged, don't know why.*

# Installation

Check the "Releases" section of the GitHub repo if you want to just download the exe and run it. Requires Windows 10.

Alternatively, you can build it yourself from the Visual Studio solution (on Windows), or by running the build.sh script on Linux.
