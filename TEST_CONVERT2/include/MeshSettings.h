#pragma once

struct MeshSettings {
    double deflection = 0.1;
    double angle = 0.5;
    bool relative = true;
    bool adaptive = true;
    int minNbTriangles = 0;
    int maxNbTriangles = 1000000;
};