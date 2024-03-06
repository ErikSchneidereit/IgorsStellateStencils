#include <cmath>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <tuple>
#include <cassert>
#include <vector>

/* For debugging purposes we can activate 
 * guarded vector access.
 */
#ifdef NDEBUG
  #define get(C, I) C[I]
#else
  #define get(C, I) C.at(I)
#endif 

/* This could also be done using the
 * numbers std library, but since we
 * know it by heart...
 */
constexpr double pi = 3.1415926535897932384626;

/* Typedefs for cleaner function signatures :)
 */
using co_t = std::pair<double, double>;
using co_vec_t = std::vector<co_t>;


/** Turns a length (s) and circumference (c) measurement
 *  plus an angular offset o
 *  into an x y coordinate pair
 */
inline co_t sctoxy (const double& s, const double& c, double o ) 
{
  co_t xy;
  xy.first = s * cos(c/s + o);
  xy.second = s * sin(c/s + o);
  return xy;
};

/** Rotates coordinates (x,y)^T by an angle alpha
 */
inline co_t rot(const double x, const double y, double alpha)
{
  co_t xy;
  xy.first = x * cos(alpha) - y * sin(alpha);
  xy.second = x * sin(alpha) + y * cos(alpha);
  return xy;
}

/** Rotates coordinates (x,y)^T by an angle alpha
 */
inline co_t rot(const co_t& xy, double alpha)
{
  return rot(xy.first, xy.second, alpha);
}

/** Mirrors a point (x,y)^T on an angle c
 */
inline co_t mirr (double x, double y, double c)
{
  co_t xy;

  double mx = cos(c);
  double my = sin(c);

  double sca = x * mx + y * my;
  
  xy.first =  2 * sca * mx - x;
  xy.second = 2 * sca * my - y;

  return xy;
};

/** Mirrors a point (x,y)^T on an angle c
 */
inline co_t mirr(const co_t& xy, double c)
{
  return mirr(xy.first, xy.second, c);
}


/** Generates the contour of the star as a length (s) and circumference (c) pair.
 *  @param R Radius of the pad
 *  @param h height of the felt
 *  @param rf fraction of the radius that overlaps
 *  @param hf fraction of the overlap to use for recovering the compressed material
 *  @param K The number of points contained in the radial part of the dataset
 *  @param K The number of jags of the star, used only to determine the number of
 *  points in the tangential part of the dataset.
 */
co_vec_t contour(const double R, const double h, const double rf, const double hf, const int K, const int N)
{
  assert(rf <= 1.0);
  assert(hf <= 1.0);
  assert(rf > 0.0);
  assert(hf > 0.0);
  assert(N > 0);
  assert(R > 0);
  assert(h >= 0);

  const double s1 = R + h;
  const double s2 = R * (1+hf*rf) + h;
  const double s3 = R * (1+rf) + h;

  auto c1 = [&] (const double& s) { return 2.0*pi*s; };
  auto c2 = [&] (const double& s) { return c1(s1) - 2.0*pi*(h + R*hf*rf)*(s-s1)/(R*hf*rf); };
  auto c3 = [&] (const double& s) { return c2(s2) - 2.0*pi*(s-s2); };

  auto c = [&] (double s)
  {
    assert(s >= 0.0);
    if (s < s1)
      return c1(s);
    else if (s < s2)
      return c2(s);
    else
      return c3(s);
  };

  const double ds = (s3 - s1) / static_cast<double>(K);
  auto sk = [&] (int k) -> double
  {
    return s1 + ds*static_cast<double>(k);
  };
  const int Kappa = floor(K + c(s3)/(ds*N*2));
  assert(Kappa >= K);

  // Fill a vector with a cut contour
  co_vec_t sc0 (Kappa);
  for (int k=0; k<K; ++k)
  {
    get(sc0,k).first = sk(k);
    get(sc0,k).second = c(sc0[k].first);
  }

  // Fill the remainder with the end contour
  for(int k=K; k<Kappa; ++k)
  {
    get(sc0,k).first = sk(K);
    get(sc0,k).second = c(sk(K)) - ds*2.0*N*(k - K);
  }
  return sc0;
}

/**Transforms a contour from (s,c) to (x,y)
 */
co_vec_t contourtoxy(const co_vec_t& sc0, const int N)
{
  const size_t I = sc0.size();
  const double frac = 0.5/static_cast<double>(N);
  co_vec_t xy0(I);
  for(size_t i=0; i<I; ++i)
  {
    xy0.at(i) = sctoxy(get(sc0,i).first, get(sc0,i).second*frac, 0.0);
  }
  return xy0;
}

/**Duplicates a (x,y) dataset representing a side of a jag to
 * make an entire star.
 */
co_vec_t xy0toN(const co_vec_t& xy0, const int N)
{
  const size_t I = xy0.size();
  const size_t IN2 = 2*N*I;
  const double dalpha = 2*pi/static_cast<double>(N);
  co_vec_t xy;
  xy.reserve(IN2);

  // Fill the first range into the overall container.
  // xy.insert_range(xy.end(), xy0);
  for(size_t n=0; n<N; ++n)
  {
    for(int i=I-1; i>=0; --i)
    {
      xy.push_back(rot(get(xy0,i), n*dalpha));
    }
    for(int i=0; i<I; ++i)
    {
      xy.push_back(mirr(rot(get(xy0,i), n*dalpha), (static_cast<double>(n)+0.5)*dalpha));
    }
  }
  assert(xy.size() == IN2);
  return xy;
}

/**Print an (x,y) dateset as an SVG file.
 */
void print(const co_vec_t& xy, std::ostream& o)
{
  o << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
    << "<svg version=\"1.1\">\n"
    << "<g\n"
    << "style=\"fill:none;stroke:#000000;stroke-opacity:1;stroke-width:0.1\">\n"
    << "<path d=\"M ";
  o << xy[0].first << " " << xy[0].second;
  for (size_t k=1; k < xy.size(); ++k)
    {
      o << " L " << xy[k].first << " " << xy[k].second;
    }
  o << "Z\"\n"
    << "id=\"path1\" />\n"
    << "</g>\n"
    << "</svg>\n"
    << std::flush;
}

/**Trivial max jag circumference length to 
 * number of jags conversion.
 */
int radtoN(double r, double cmax)
{
  return static_cast<int>(ceil(2*pi*r/cmax));
}

/**The main function.
 */
int main (int argc, char* argv [])
{
  assert(argc == 2);
  std::ifstream in(argv[1]);
  double res, h, cmax, rmin, drmax, hf;
  in >> res >> h >> cmax >> rmin >> drmax >> hf;
  double R;
  in >> R;
  while (!in.eof())
  {
    std::cout << "Generating star for R = "
              << R << std::endl;

    int N = radtoN(R, cmax);
    int K = R/res;
    double rf = std::min(R - rmin, drmax)/R;

    std::ostringstream s;
    s << std::setprecision(1) << std::fixed << R;
    std::ofstream o (s.str() + ".svg");
    co_vec_t sc0 = contour(R, h, rf, hf, K, N);
    co_vec_t xy0 = contourtoxy(sc0, N);
    co_vec_t xyN = xy0toN(xy0, N);
    print(xyN, o);
    in >> R;
  }
  return 0;
}
