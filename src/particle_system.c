#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <stdio.h>
#include <string.h>
#include "particle_system.h"

double rand01() {
  return (double)rand() / RAND_MAX;
}

void update_particles(ParticleSystem* ps, double delta_time) {
  for (int i = 0; i < ps->particle_count; i++) {
    Particle* particle = &ps->particles[i];

    particle->travel_time_secs += delta_time;
    
    if (particle->config == NULL) continue;

    particle->y_velocity += particle->config->gravity_force * delta_time;

    particle->x += particle->x_velocity * delta_time * particle->config->particle_speed;
    particle->y += particle->y_velocity * delta_time * particle->config->particle_speed;

    if (particle->travel_time_secs > particle->config->max_lifetime) {
      Particle temp = ps->particles[ps->particle_count - 1];
      ps->particles[ps->particle_count - 1] = *particle;
      *particle = temp;

      ps->particle_count -= 1;
      i -= 1;
    }
  }
}

void spawn_particles(ParticleSystem* ps, double delta_time) {
  for (int i = 0; i < ps->particle_count; i++) {
    Particle* particle = &ps->particles[i]; 

    if (particle->emitting_config == NULL) continue;
    if (particle->travel_time_secs == 0) continue;

    for (int j = 0; j < particle->emitting_config_count; j++) {
      EmitterConfig* particle_emitting_config = &particle->emitting_config[j];
      ParticleEmitter* particle_emitter = &particle->emitters[j];

      double range = particle_emitting_config->start_angle_rad - particle_emitting_config->end_angle_rad; 
      
      double particles_per_second = particle_emitter->emit_count / particle->travel_time_secs;

      double target_spawn_frequency = particle_emitting_config->spawn_frequency;

      if (
        particle_emitting_config->type == ET_BURST 
      ) {
        double time_differance = particle_emitter->last_burst_time + particle_emitting_config->burst_interval - particle->travel_time_secs;
        if (time_differance < 0) {
          particle_emitter->last_burst_time = particle->travel_time_secs + time_differance;
        } else continue;
      }

      while (particles_per_second < target_spawn_frequency) {
        double angle = rand01() * range + particle_emitting_config->start_angle_rad;
        int type = rand01() * particle_emitting_config->sub_emmissions_count;

        Particle new_particle;

        new_particle.travel_time_secs = 0;
        new_particle.y_velocity = cos(angle);
        new_particle.x_velocity = sin(angle);
        new_particle.x = particle->x;
        new_particle.y = particle->y;
        new_particle.config = particle_emitting_config;

        if (particle_emitting_config->amount == EA_SINGLE) {
          new_particle.emitting_config = &particle_emitting_config->sub_emmissions[type];
          new_particle.emitting_config_count = 1; 
          new_particle.emitters[0].emit_count = 0;
          new_particle.emitters[0].last_burst_time = 0;
        } else {
          new_particle.emitting_config = particle_emitting_config->sub_emmissions;
          new_particle.emitting_config_count = particle_emitting_config->sub_emmissions_count; 
        
          for (int k = 0; k < particle_emitting_config->sub_emmissions_count; k++) {
            new_particle.emitters[k].emit_count = 0;
            new_particle.emitters[k].last_burst_time = 0;
          }
        }

        ps->particles[ps->particle_count] = new_particle; 

        ps->particle_count += 1;
        particle_emitter->emit_count += 1;
        
        particles_per_second = particle_emitter->emit_count / particle->travel_time_secs;
      }
    }
  }
}

void render_particles(ParticleSystem* ps, SDL_Renderer* renderer, SDL_Texture* particle_image) {
  for (int i = 0; i < ps->particle_count; i++) {
    Particle* particle = &ps->particles[i];
    
    if (particle->config == NULL) continue;

    EmitterConfig* particle_config = particle->config;

    const double half_particle_size = particle_config->particle_size / 2.0f;

    int total = 0;

    for (int i = 0; i < particle_config->color_stop_count - 1; i++) {
      total += ps->config->color_stops[i].ratio;
    }

    SDL_Rect render_rect;
    render_rect.x = particle->x - half_particle_size;
    render_rect.y = particle->y - half_particle_size;
    render_rect.w = half_particle_size * 2.0f;
    render_rect.h = half_particle_size * 2.0f;

    const double age = particle->travel_time_secs / particle->config->max_lifetime;
    double min_age = 0;
    
    for (int j = 0; j < particle_config->color_stop_count - 1; j++) {
      const double current_age_addition = particle_config->color_stops[j].ratio / (double)total;
      const double max_age = current_age_addition + min_age;
 
      if (age < min_age || age > max_age) {
        min_age += current_age_addition;
        continue; 
      }

      const double min_delta = age - min_age;
      const double max_delta = max_age - age;

      const double scale_amount = 1 / current_age_addition;
    
      const double max_ratio = scale_amount * max_delta;
      const double min_ratio = scale_amount * min_delta;

      int next = j+1;
      
      Uint8 red = particle_config->color_stops[next].r * min_ratio + particle_config->color_stops[j].r * max_ratio;
      Uint8 green = particle_config->color_stops[next].g * min_ratio + particle_config->color_stops[j].g * max_ratio;
      Uint8 blue = particle_config->color_stops[next].b * min_ratio + particle_config->color_stops[j].b * max_ratio;

      SDL_SetTextureColorMod(particle_image, red, green, blue);

      Uint8 alpha = particle_config->color_stops[next].a * min_ratio + particle_config->color_stops[j].a * max_ratio;

      SDL_SetTextureAlphaMod(particle_image, alpha); 

      min_age = min_age + current_age_addition;
    }

    SDL_RenderCopy(renderer, particle_image, NULL, &render_rect);  
    
    SDL_SetTextureColorMod(particle_image, 255, 255, 255);

    SDL_SetTextureAlphaMod(particle_image, 255); 
  }
}

void print_color_stop(const ColorStop* color_stop) {
  printf(
    "r: %i, g: %i b: %i, a: %i\n", 
    color_stop->r, 
    color_stop->g, 
    color_stop->b, 
    color_stop->a
  );
} 

void print_emit_type(EMIT_TYPE type) {
  switch (type) {
    case ET_NONE:
      printf("ET_NONE\n");
    break;
    case ET_CONTINOUS:
      printf("ET_CONTINOUS\n");
    break;
    case ET_BURST:
      printf("ET_BURST\n");
    break;
    default:
      printf("Unhandled Case\n");
    break;
  }
}

void print_emitter_config(const EmitterConfig* ec) {
  printf("max_lifetime %f\n", ec->max_lifetime);
  printf("spawn_frequency %f\n", ec->spawn_frequency);
  printf("start_angle_rad %f\n", ec->start_angle_rad);
  printf("end_angle_rad %f\n", ec->end_angle_rad);
  printf("particle_size %i\n", ec->particle_size);
  printf("particle_speed %f\n", ec->particle_speed);
  printf("gravity_force %f\n", ec->gravity_force);

  printf("color_stop_count %i\n", ec->color_stop_count);

  for (int i = 0; i < ec->color_stop_count; i++) {
    printf("index: %i | ", i); 
    print_color_stop(&ec->color_stops[i]);
  }

  print_emit_type(ec->type);

  printf("child_count %i\n", ec->sub_emmissions_count);
  
  for (int i = 0; i < ec->sub_emmissions_count; i++) {
    printf("\nchild %i\n\n", i);
    print_emitter_config(&ec->sub_emmissions[i]);
    printf("\n");
  }
}

void init_particle_system(ParticleSystem* ps, EmitterConfig* ec) {
  ps->config = ec;
  ps->particle_count = 1;

  Particle root_emitter;
  root_emitter.x = 600 / 2.0f;
  root_emitter.y = 600 / 2.0f;
  root_emitter.x_velocity = 0;
  root_emitter.y_velocity = 0;
  root_emitter.config = NULL;
  root_emitter.emitting_config = ps->config;
  root_emitter.emitters[0].last_burst_time = 0;
  root_emitter.emitters[0].emit_count = 0;
  root_emitter.travel_time_secs = 0;
  root_emitter.emitting_config_count = 1;

  ps->particles[0] = root_emitter;
}

void update_particle_system(ParticleSystem* ps, SDL_Renderer* renderer, SDL_Texture* particle_image, double delta_time) {
  spawn_particles(ps, delta_time);
  update_particles(ps, delta_time);
  render_particles(ps, renderer, particle_image);
}
