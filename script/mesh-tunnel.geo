SetFactory("OpenCASCADE");

// Tunnel dimension
H = 0.41;
L = 2.5;

// Cilinder dimension and position
r = 0.05;
y = 0.2;
x = 0.5;

// Mesh size
N = 32;
h = 1.0 / N;

// Outer points
Point(0) = {0, 0, 0, h};
Point(1) = {L, 0, 0, h};
Point(2) = {L, H, 0, h};
Point(3) = {0, H, 0, h};

// Outer lines
Line(0) = {0, 1};
Line(1) = {1, 2};
Line(2) = {2, 3};
Line(3) = {3, 0};
Curve Loop(0) = {0, 1, 2, 3};

// Circle line
Circle(4) = {x, y, 0, r, 0, 2*Pi};
MeshSize {4} = h / 4;
Curve Loop(1) = {4};

Plane Surface(0) = {0, 1};

// Generate volume
Extrude {0, 0, H} { Surface{0}; }


// Wall surface
Physical Surface(0) = {0, 1, 3, 6, 5};

// Inlet surface
Physical Surface(1) = {4};

// Outlet surface
Physical Surface(2) = {2};

// Physical volume
Physical Volume(0) = {1};


// Saving mesh to file
strN = Sprintf("%.0f", N);
Mesh 3;
Save StrCat("../mesh/mesh-step-", strN, ".msh");
