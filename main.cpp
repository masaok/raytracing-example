/*
 * main.cpp
 *
 *  Created on: Jun 5, 2014
 *      Author: Rodrigo Costa
 *			e-mail: rodrigocosta@telemidia.puc-rio.br
 */

#include <math.h>

#include <iostream>
#include <set>
#include <string>

#include "Color.h"
#include "IShape.h"
#include "Light.h"
#include "Vector3D.h"
#include "util.h"

#define MAX_DEPTH 8

extern const float bias;

IShape *calculateIntersect(const Ray &ray, std::set<IShape *> &sceneShapes,
                           float *t, Vector3D &shapeNormal, Color &pixelColor) {
  *t = INFINITY;
  Color color;
  Vector3D normal;
  IShape *shapeIntersection = 0;
  for (auto shape : sceneShapes) {
    float near;
    if (shape->intersect(ray, &near, normal, color) && near < *t) {
      shapeNormal = normal;
      pixelColor = color;
      *t = near;
      shapeIntersection = shape;
    }
  }
  return shapeIntersection;
}

Color ambientColor(const Color &color) {
  return ambientCoefficient * color;
}

Color specularColor(const Vector3D &direction, const Vector3D &normal,
                    const Ray &ray, const Light *light,
                    const float &specularCoefficient) {
  Color specularColor;
  Vector3D refl = direction - normal * 2 * Vector3D(direction).dot(normal);
  refl.normalize();

  float m = refl.dot(ray.direction());
  m = m < 0 ? 0.0f : m;

  float cosB = m / refl.length() * ray.direction().length();
  float spec = pow(cosB, 50);

  specularColor = specularCoefficient * light->color() * spec;

  return specularColor;
}

Color diffuseColor(const Vector3D &direction, const Light *light,
                   const Vector3D &normal, const Color &color,
                   const float &diffuseCoefficient) {
  Color diffuseColor;
  float dot = Vector3D(normal).dot(direction);
  dot = dot < 0 ? 0.0f : dot;

  diffuseColor = diffuseCoefficient * color * light->color() * dot;

  return diffuseColor;
}

Color trace(const Ray &ray, std::set<IShape *> &sceneShapes,
            std::set<Light *> &sceneLights, int depth) {
  Color pixelColor(0.3);

  float near;
  Color color;
  Vector3D normal;
  IShape *shape = calculateIntersect(ray, sceneShapes, &near, normal, color);
  if (shape) {
    pixelColor = color;
    Point intersectionPoint = ray.calculate(near);

    Vector3D n;
    Color c;

    pixelColor = Color(0.0f);

    //Calculate illumination on intersected pixel
    for (auto light : sceneLights) {
      Vector3D lightDirection = (light->position() - intersectionPoint);

      float lightLenght = lightDirection.normalize();

      const Ray shadowRay(intersectionPoint + normal * bias, lightDirection,
                          lightLenght);
      float near = INFINITY;

      IShape *s = calculateIntersect(shadowRay, sceneShapes, &near, n, c);
      if (!s)  //There is no object between the intersected pixel and this light.
      {
        float diffuseCoefficient = shape->diffuse();
        float specularCoefficient = shape->specular();

        pixelColor += ambientColor(color);
        if (diffuseCoefficient > 0.0f)
          pixelColor += diffuseColor(lightDirection, light, normal, color,
                                     diffuseCoefficient);

        if (specularCoefficient > 0.0f)
          pixelColor += specularColor(lightDirection, normal, ray, light,
                                      specularCoefficient);
      } else  //Intersected pixel is shadowed!!!
      {
        pixelColor = color * 0.1;
        break;
      }
    }

    //Calculate the reflected color
    if ((shape->reflection() > 0) && depth <= MAX_DEPTH) {
      Vector3D reflDir = ray.direction() - normal * 2 * ray.direction().dot(normal);
      reflDir.normalize();

      Ray reflectionRay(intersectionPoint + normal * bias, reflDir);
      Color reflectionColor = trace(reflectionRay, sceneShapes, sceneLights,
                                    depth + 1);

      pixelColor += reflectionColor * shape->reflection();
    }
  }

  pixelColor.clamp();
  return pixelColor;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: ./RayTracerT3 <widht> <height> [<fov>]\n");
    return 0;
  }

  int width = atoi(argv[1]);
  ;
  int height = atoi(argv[2]);
  ;

  float fov = 60.0;
  if (argc >= 4) {
    fov = atof(argv[3]);
  }

  std::set<IShape *> sceneShapes;
  std::set<Light *> sceneLights;

  initScene(sceneShapes, sceneLights);

  float tanFov = tan(fov * 0.5 * M_PI / 180.0f);

  float aspectratio = float(width) / float(height);
  Point origin(0.0f, 5.0f, 20.0f);

#ifdef SDL_SUPPORT
  SDL_Window *window;
  SDL_Renderer *render;
  SDL_Surface *surface;
  SDL_Texture *texture;
  bool sdlOk = sdlBootstrap(&window, &render, &surface, &texture, width,
                            height);
  Uint32 *pixels = new Uint32[width * height];

#else
  std::string filename = "out.ppm";
  Image *image = new Image[width * height];
#endif

  printf("Rendering scene:\n");
  printf("Width: %d \nHeight: %d\nFov: %.2f\n", width, height, fov);

  for (int y = 0; y < height; y++) {
    printf("\r%.2f%%", float(y) / height * 100);

    Color color;
    for (int x = 0; x < width; x++) {
      float yu = (1 - 2 * ((y + 0.5) * 1 / height)) * tanFov;
      float xu = (2 * ((x + 0.5) * 1 / float(width)) - 1) * tanFov * aspectratio;
      Ray ray(origin, Vector3D(xu, yu, -1));

      color = trace(ray, sceneShapes, sceneLights, 0);
      //
      //			yu = (1 - 2 * ((y - 0.5) * 1 / height)) * tanFov;
      //			xu = (2 * ((x - 0.5) * 1 / float (width)) - 1) * tanFov * aspectratio;
      //
      //			ray = Ray (origin, Vector3D (xu, yu, -1));
      //			color += trace (ray, sceneShapes, sceneLights, 0);
      //
      //			yu = (1 - 2 * ((y) * 1 / height)) * tanFov;
      //			xu = (2 * ((x - 0.5) * 1 / float (width)) - 1) * tanFov * aspectratio;
      //
      //			ray = Ray (origin, Vector3D (xu, yu, -1));
      //			color += trace (ray, sceneShapes, sceneLights, 0);
      //
      //			yu = (1 - 2 * ((y + 0.5) * 1 / height)) * tanFov;
      //			xu = (2 * ((x - 0.5) * 1 / float (width)) - 1) * tanFov * aspectratio;
      //
      //			ray = Ray (origin, Vector3D (xu, yu, -1));
      //			color += trace (ray, sceneShapes, sceneLights, 0);
      //
      //			yu = (1 - 2 * ((y - 0.5) * 1 / height)) * tanFov;
      //			xu = (2 * ((x) * 1 / float (width)) - 1) * tanFov * aspectratio;
      //
      //			ray = Ray (origin, Vector3D (xu, yu, -1));
      //			color += trace (ray, sceneShapes, sceneLights, 0);
      //
      //			yu = (1 - 2 * ((y) * 1 / height)) * tanFov;
      //			xu = (2 * ((x) * 1 / float (width)) - 1) * tanFov * aspectratio;
      //
      //			ray = Ray (origin, Vector3D (xu, yu, -1));
      //			color += trace (ray, sceneShapes, sceneLights, 0);
      //
      //			yu = (1 - 2 * ((y + 0.5) * 1 / height)) * tanFov;
      //			xu = (2 * ((x) * 1 / float (width)) - 1) * tanFov * aspectratio;
      //
      //			ray = Ray (origin, Vector3D (xu, yu, -1));
      //			color += trace (ray, sceneShapes, sceneLights, 0);
      //
      //			yu = (1 - 2 * ((y - 0.5) * 1 / height)) * tanFov;
      //			xu = (2 * ((x + 0.5) * 1 / float (width)) - 1) * tanFov * aspectratio;
      //
      //			ray = Ray (origin, Vector3D (xu, yu, -1));
      //			color += trace (ray, sceneShapes, sceneLights, 0);
      //
      //			yu = (1 - 2 * ((y) * 1 / height)) * tanFov;
      //			xu = (2 * ((x + 0.5) * 1 / float (width)) - 1) * tanFov * aspectratio;
      //
      //			ray = Ray (origin, Vector3D (xu, yu, -1));
      //			color += trace (ray, sceneShapes, sceneLights, 0);
      //
      //			yu = (1 - 2 * ((y + 0.5) * 1 / height)) * tanFov;
      //			xu = (2 * ((x + 0.5) * 1 / float (width)) - 1) * tanFov * aspectratio;
      //
      //			ray = Ray (origin, Vector3D (xu, yu, -1));
      //			color += trace (ray, sceneShapes, sceneLights, 0);
      //
      //			color /= 9;

#ifdef SDL_SUPPORT
      if (sdlOk) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        pixels[y * width + x] = SDL_MapRGBA(surface->format,
                                            Uint8(color.r() * 255),
                                            Uint8(color.g() * 255),
                                            Uint8(color.b() * 255), Uint8(255));
#else
        pixels[y * width + x] = SDL_MapRGBA(surface->format,
                                            Uint8(color.b() * 255),
                                            Uint8(color.g() * 255),
                                            Uint8(color.r() * 255),
                                            Uint8(255));
#endif
      }
#else
      image[y * width + x] = color;
#endif
    }

#ifdef SDL_SUPPORT
    if (sdlOk) {
      SDL_UpdateTexture(texture, NULL, pixels, width * sizeof(Uint32));
      SDL_RenderClear(render);
      SDL_RenderCopy(render, texture, NULL, NULL);
      SDL_RenderPresent(render);
    }
#endif
  }

  printf("\nFinished!\n");

#ifdef SDL_SUPPORT
  if (sdlOk) {
    SDL_RenderPresent(render);

    bool quit = false;

    SDL_Event event;
    while (!quit) {
      SDL_WaitEvent(&event);
      switch (event.type) {
        case SDL_QUIT:
          quit = true;
          break;
      }
    }
  }
#else
  {
    writePPMFile(image, filename.c_str(), width, height);
    system(std::string("eog " + filename).c_str());
  }
#endif

  std::set<IShape *>::iterator it = sceneShapes.begin();
  while (it != sceneShapes.end()) {
    free(*it);
    sceneShapes.erase(it);
    it++;
  }

  std::set<Light *>::iterator it2 = sceneLights.begin();
  while (it2 != sceneLights.end()) {
    free(*it2);
    sceneLights.erase(it2);
    it2++;
  }

#ifdef SDL_SUPPORT
  delete[] pixels;

  if (sdlOk) {
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(render);
    SDL_DestroyWindow(window);
  }
#else
  delete image;
#endif

  return 0;
}
