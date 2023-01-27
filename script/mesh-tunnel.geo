SetFactory("OpenCASCADE");

// Tunnel dimension
H = 0.41;
L = 2.5;

// Cilinder dimension and position
r = 0.05;
y = 0.2;
x = 0.5;

// Mesh size
N = 16.0;
h = 1.0 / N;

// Outer point
Point(1) = {0, 0, 0, h};
Point(2) = {L, 0, 0, h};
Point(3) = {L, H, 0, h};
Point(4) = {0, H, 0, h};

// Outer lines
Line(1) = {1, 2};
Line(2) = {2, 3};
Line(3) = {3, 4};
Line(4) = {4, 1};

// Circle
Circle(5) = {x, y, 0, r, 0, 2*Pi};
MeshSize {5} = h / 4.0;

// Side surface
Curve Loop(1) = {3, 4, 1, 2};
Curve Loop(2) = {5};
Plane Surface(1) = {1, 2};

// Extrude volume
Extrude {0, 0, H / 2.0} {
  Surface{1}; 
}

// Inlet surface
Physical Surface(1) = {3};
// Wall surface
Physical Surface(2) = {1, 7, 4, 2};
// Cylinder surface
Physical Surface(3) = {6};
// Outlet surface
Physical Surface(4) = {5};

// Physical domain
Physical Volume(1) = {1};

// Saving mesh to file
strN = Sprintf("%.0f", N);
Mesh 3;
Save StrCat("../mesh/mesh-tunnel-step-", strN, ".msh");
