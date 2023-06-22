#ifndef OCTAHEDRAL_MAP_GLSL
#define OCTAHEDRAL_MAP_GLSL

// Converts normalized direction to the octahedral map (non-equal area, signed normalized).
//  - n: Normalized direction.
//  - returns: Position in octahedral map in [-1,1] for each component.
vec2 normalized_vector_to_octahedral_mapping(vec3 n)
{
    // Project the sphere onto the octahedron (|x|+|y|+|z| = 1) and then onto the xy-plane.
    vec2 p = vec2(n.x, n.y) * (1.0 / (abs(n.x) + abs(n.y) + abs(n.z)));

    // Reflect the folds of the lower hemisphere over the diagonals.
    if (n.z < 0.0)
    {
        p = vec2((1.0 - abs(p.y)) * (p.x >= 0.0 ? 1.0 : -1.0),
                 (1.0 - abs(p.x)) * (p.y >= 0.0 ? 1.0 : -1.0));
    }
    return p;
}

// Converts point in the octahedral map to normalized direction (non-equal area, signed normalized).
//  - p: Position in octahedral map in [-1,1] for each component.
//  - returns: Normalized direction.
vec3 octahedral_mapping_to_normalized_vector(vec2 p)
{
    vec3 n = vec3(p.x, p.y, 1.0 - abs(p.x) - abs(p.y));

    // Reflect the folds of the lower hemisphere over the diagonals.
    if (n.z < 0.0)
    {
        n.xy = vec2((1.0 - abs(n.y)) * (n.x >= 0.0 ? 1.0 : -1.0),
                    (1.0 - abs(n.x)) * (n.y >= 0.0 ? 1.0 : -1.0));
    }
    return normalize(n);
}

#endif // OCTAHEDRAL_MAP_GLSL