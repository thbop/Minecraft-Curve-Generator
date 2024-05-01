// #include "stdio.h"
#include <string.h> 
#include "raylib.h"
#include "raymath.h"

#define CURVERESOLUTION 64
#define AMOUNTOFSHAPES 7


#define WORLDWIDTH 16
#define WORLDHEIGHT 9
#define WORLDTOTAL WORLDWIDTH*WORLDHEIGHT

const int
    BLOCKSIZE      = 64,
    QUADDELTA      = BLOCKSIZE >> 1, // divide by 2
    WIDTH          = BLOCKSIZE*WORLDWIDTH,
    HEIGHT         = BLOCKSIZE*WORLDHEIGHT,
    MINCURVEPOINTS = 2;

// { top right, top left, bottom right, bottom left }
const bool shapes[][4] = {
    { 1, 1, 1, 1 }, // Full block
    { 0, 1, 1, 1 }, // Stairs
    { 1, 0, 1, 1 },
    { 1, 1, 0, 1 },
    { 1, 1, 1, 0 },
    { 0, 0, 1, 1 }, // Slabs
    { 1, 1, 0, 0 }
};

struct Vector2i { int x, y; } typedef Vector2i;


Vector2 curveCache[CURVERESOLUTION];
Vector2i intersectionCache[WORLDTOTAL];

Vector2 mouse;
int intersections = 0;

Vector2 Vector2MultiplyValue( float x, Vector2 v ) {
    return (Vector2){ x * v.x, x * v.y };
}

// t is from 0 to 1
Vector2 lerp( Vector2 p0, Vector2 p1, float t ) {
    return Vector2Add(
        Vector2MultiplyValue( (1 - t), p0 ),
        Vector2MultiplyValue( t,       p1 )
    );
}

// Two points, two curve handles, and time
Vector2 DeCasteljau( Vector2 p0, Vector2 h0, Vector2 p1, Vector2 h1, float t ) {
    Vector2 A, B, C, D, E;
    A = lerp( p0, h0, t );
    B = lerp( h0, h1, t );
    C = lerp( h1, p1, t );
    D = lerp( A, B, t );
    E = lerp( B, C, t );
    return lerp( D, E, t ); // return P
}

// Draws curve by approximating with line segments
void DrawCurve( Vector2 p0, Vector2 h0, Vector2 p1, Vector2 h1, Color color ) {
    float
        t     = 0.0f,
        delta = 1.0f / CURVERESOLUTION;

    Vector2 s0, s1;
    s0 = DeCasteljau( p0, h0, p1, h1, t );
    s1 = DeCasteljau( p0, h0, p1, h1, t+delta );
    curveCache[0] = s0;
    for ( int i = 0; i < CURVERESOLUTION; i++ ) {
        s0 = s1;
        s1 = DeCasteljau( p0, h0, p1, h1, t+delta );
        DrawLineV( s0, s1, color );
        t += delta;

        if ( i < 63 ) curveCache[i+1] = s1;
    }
}


void DrawBlockGrid( Color color ) {
    for ( int i = 1; i < WORLDWIDTH; i++ )
        DrawLine( i*BLOCKSIZE, 0, i*BLOCKSIZE, HEIGHT, color );
    for ( int j = 1; j < WORLDWIDTH; j++ )
        DrawLine( 0, j*BLOCKSIZE, WIDTH,  j*BLOCKSIZE, color );
}

void DrawShape( Vector2i pos, int shapeIndex, Color color ) {
    int
        i = BLOCKSIZE*pos.x,
        j = BLOCKSIZE*pos.y;
    
    if ( shapes[shapeIndex][0] ) DrawRectangle( i,           j,           QUADDELTA, QUADDELTA, color );
    if ( shapes[shapeIndex][1] ) DrawRectangle( i+QUADDELTA, j,           QUADDELTA, QUADDELTA, color );
    if ( shapes[shapeIndex][2] ) DrawRectangle( i,           j+QUADDELTA, QUADDELTA, QUADDELTA, color );
    if ( shapes[shapeIndex][3] ) DrawRectangle( i+QUADDELTA, j+QUADDELTA, QUADDELTA, QUADDELTA, color );
}

int CheckIntersections() {
    int
        inter = 0,
        dinter = 0;
    for ( int j = 0; j < WORLDHEIGHT; j++ ) {
        for ( int i = 0; i < WORLDWIDTH; i++ ) {
            for ( int p = 0; p < CURVERESOLUTION; p++ ) {
                Rectangle rec = { (float)i*BLOCKSIZE, (float)j*BLOCKSIZE, (float)BLOCKSIZE, (float)BLOCKSIZE };
                if ( CheckCollisionPointRec( curveCache[p], rec ) ) {
                    intersectionCache[inter] = (Vector2i){i, j};
                    dinter++;
                }
            }
            if ( dinter > MINCURVEPOINTS ) inter += dinter;
            dinter = 0;
        }
    }
    return inter;
}

int GetShape( Vector2i pos ) {
    int
        i = BLOCKSIZE*pos.x,
        j = BLOCKSIZE*pos.y;
    
    float shape[4] = { 0.0f };

    Rectangle quad0 = { (float)i,           (float)j,           (float)QUADDELTA, (float)QUADDELTA };
    Rectangle quad1 = { (float)i+QUADDELTA, (float)j,           (float)QUADDELTA, (float)QUADDELTA };
    Rectangle quad2 = { (float)i,           (float)j+QUADDELTA, (float)QUADDELTA, (float)QUADDELTA };
    Rectangle quad3 = { (float)i+QUADDELTA, (float)j+QUADDELTA, (float)QUADDELTA, (float)QUADDELTA };

    int total = 0;

    for ( int p = 0; p < CURVERESOLUTION; p++ ) if (CheckCollisionPointRec( curveCache[p], quad0 )) { shape[0]++; total++; }
    for ( int p = 0; p < CURVERESOLUTION; p++ ) if (CheckCollisionPointRec( curveCache[p], quad1 )) { shape[1]++; total++; }
    for ( int p = 0; p < CURVERESOLUTION; p++ ) if (CheckCollisionPointRec( curveCache[p], quad2 )) { shape[2]++; total++; }
    for ( int p = 0; p < CURVERESOLUTION; p++ ) if (CheckCollisionPointRec( curveCache[p], quad3 )) { shape[3]++; total++; }

    float smallestDis;
    int bestFitIndex = 0;
    for ( int s = 0; s < AMOUNTOFSHAPES; s++ ) {
        int mcsize = 0;
        for ( int n = 0; n < 4; n++ ) if ( shapes[s][n] ) mcsize++;
        float fragsize = total / mcsize;
        float mcshape[4] = { 0.0f };
        for ( int n = 0; n < 4; n++ ) if ( shapes[s][n] ) mcshape[n] = fragsize;

        float
            a   = shape[0] - mcshape[0],
            b   = shape[1] - mcshape[1],
            c   = shape[2] - mcshape[2],
            d   = shape[3] - mcshape[3],
            dis = a*a + b*b + c*c + d*d; // Finding the distance squared; no point in sqrt; bad approx, but whatever...
        
        if ( !s )                     { smallestDis = dis;                   }
        else if ( dis < smallestDis ) { smallestDis = dis; bestFitIndex = s; }
    }
    // printf("%f %f %f %f\n", mcshape[0], mcshape[1], mcshape[2], mcshape[3]);
    // printf("\n%f %f %f %f\n\n", shape[0], shape[1], shape[2], shape[3]);
    return bestFitIndex;
}

Vector2 points[4] = {
    { 20.0f, 20.0f },
    { 100.0f, 500.0f },
    { 600.0f, 200.0f },
    { 400.0f, 400.0f }
};
bool selectMap[4] = {0};


void draw() {
    for ( int i = 0; i < intersections; i++ ) {
        int shapeIndex = GetShape( intersectionCache[i] );
        DrawShape( intersectionCache[i], shapeIndex, RED );
    }

    DrawBlockGrid( GRAY );

    DrawCurve( points[0], points[1], points[2], points[3], WHITE );

    DrawLineV( points[0], points[1], GRAY );
    DrawLineV( points[2], points[3], GRAY );

    DrawCircleV( points[0], 5.0f, WHITE );
    DrawCircleV( points[1], 5.0f, BLUE  );
    DrawCircleV( points[2], 5.0f, WHITE );
    DrawCircleV( points[3], 5.0f, BLUE  );
    
}

void update() {
    mouse = GetMousePosition();
    intersections = CheckIntersections();

    if ( IsMouseButtonDown(0) ) {
        for ( int i = 0; i < 4; i++ ) {
            if ( CheckCollisionPointCircle( mouse, points[i], 5.0f ) ) selectMap[i] = true;
            if ( selectMap[i] ) points[i] = mouse;
        }
    } else memset(selectMap, false, 4*sizeof(bool));
}

int main() {
    InitWindow( WIDTH, HEIGHT, "Minecraft Curve Generator" );

    while (!WindowShouldClose()) {
        update();
        BeginDrawing();
            ClearBackground(BLACK);
            draw();
        EndDrawing();
    }
    return 0;
}
