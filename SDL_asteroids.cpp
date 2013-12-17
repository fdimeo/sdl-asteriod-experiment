/*
SDL2_test.c
Testing SDL2 with Linux 
http://nothingtocode.blogspot.com
*/

#include <SDL2/SDL.h>
#include <cstdbool>
#include <iostream>
#include <unordered_set>

/* Window resolution */
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

/* Window title */
#define WINDOW_TITLE "SDL2 Test"

/* The window */
SDL_Window* window = NULL;
	
/* The window surface */
SDL_Surface* screen = NULL;


/* The event structure */
SDL_Event event;

/* The game loop flag */
_Bool running = true;

SDL_Renderer* Main_Renderer;
SDL_Texture* bkgd_texture;

SDL_sem *stop_refresh_timer, *refresh_timer_stopped;
SDL_sem *stop_game_timer, *game_timer_stopped;



// typedefs of various necessary elements
typedef std::pair<unsigned int, unsigned int> position;
typedef std::pair<double, double> velocity;
enum class direction { LEFT, RIGHT};

// typesafe constants
const unsigned int TIMER_UPDATE = static_cast<int> (round(1000/60));
const unsigned int GAME_UPDATE = static_cast<int> (1000);
const unsigned int MAX_NUM_ASTEROIDS = static_cast<int> (12);


// the asteroid class for creating, updating and rendering an asteroid
class asteroid
{
private:
   SDL_Texture *pSprite_texture;
   double a_vel;
   velocity vel;
   direction rotation_dir;
   double angle;
   const SDL_Rect srcrect = {
      .x = 0,
      .y = 0,
      .w = 90,
      .h = 90
   };
   SDL_Rect dstrect;
   SDL_Surface *tmpsurf = 0;
   
public:
   asteroid(position pos, velocity vel, double a_vel, direction rotational_direction);
   ~asteroid();
   void update();
   void render(SDL_Renderer *pRenderer);
};

// helper function that updates and renders all asteroids that are currently alive
void update_all_asteroids(std::unordered_set< asteroid* > all_asteroids)
{
   for (auto it : all_asteroids){
      it->update();
      it->render(Main_Renderer);
   }
}

double operator *(double d, direction dir)
{
   return (d * (dir == direction::LEFT ? -1.0 : 1.0));
}

asteroid::asteroid(position pos, velocity vel, double a_vel, direction rotational_direction):a_vel(a_vel),vel(vel),rotation_dir(rotational_direction)
{
   tmpsurf = SDL_LoadBMP( "asteroid_blue.bmp" );

   dstrect.x = pos.first;
   dstrect.y = pos.second;
   dstrect.w = 90;
   dstrect.h = 90;
   
   // the initial angle is zero
   angle=0;
}

asteroid::~asteroid()
{
   SDL_DestroyTexture(pSprite_texture);
}

void asteroid::update()
{
   dstrect.x += vel.first;
   dstrect.y += vel.second;
   
   angle += (a_vel * rotation_dir);
   
}

void asteroid::render(SDL_Renderer *pRenderer)
{
   if(tmpsurf){
      // make a texture from the surface the first time we're called.
      pSprite_texture = SDL_CreateTextureFromSurface(pRenderer, tmpsurf);
      SDL_FreeSurface(tmpsurf);
      tmpsurf = static_cast <SDL_Surface *> (NULL);
   }

   SDL_RenderCopyEx(pRenderer, pSprite_texture, &srcrect, &dstrect, angle, NULL, SDL_FLIP_NONE);
}




std::unordered_set<asteroid *> all_asteroids;


Uint32 update_game(Uint32 interval, void *param)
{
   // run the timer until the main thread signals up to stop
   if(!SDL_SemValue(stop_game_timer)){
      
      if (all_asteroids.size() < MAX_NUM_ASTEROIDS)
      {
         all_asteroids.insert(new asteroid(std::make_pair(50,300), std::make_pair(1,-1), 2.4, direction::RIGHT));
      }
   }

   else{
      std::cout << "game timer stopped" << std::endl;

      // we're done, destroy all the spawned asteroids
      all_asteroids.clear();

      // we've been requested to stop this timer...so tell the main thread that we have
      SDL_SemPost(game_timer_stopped);
   }

   return interval;

}


Uint32 update_screen(Uint32 interval, void *param)
{
   // run the timer until the main thread signals up to stop
   if(!SDL_SemValue(stop_refresh_timer)){

      SDL_RenderCopy(Main_Renderer, bkgd_texture, NULL, NULL);
   
      update_all_asteroids(all_asteroids);

      SDL_RenderPresent(Main_Renderer);
   }
   else{
      std::cout << "refresh timer stopped" << std::endl;
      // we've been requested to stop this timer...so tell the main thread that we have
      SDL_SemPost(refresh_timer_stopped);
   }

   return interval;
}




int main( int argc, char* args[] )
{
   SDL_TimerID refresh_timer, game_timer;
   
   stop_refresh_timer = SDL_CreateSemaphore(0);
   refresh_timer_stopped = SDL_CreateSemaphore(0);

   stop_game_timer = SDL_CreateSemaphore(0);
   game_timer_stopped = SDL_CreateSemaphore(0);


   if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0 )
   {
      printf( "SDL2 could not initialize! SDL2_Error: %s\n", SDL_GetError() );
   }
   else
   {
      window = SDL_CreateWindow(
         WINDOW_TITLE,
         SDL_WINDOWPOS_CENTERED,
         SDL_WINDOWPOS_CENTERED,
         WINDOW_WIDTH,
         WINDOW_HEIGHT,
         SDL_WINDOW_SHOWN);

   std::cout << "updating screen every " << TIMER_UPDATE << " mS" << std::endl;
   
   Main_Renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

   SDL_Surface *tmpsurf = SDL_LoadBMP( "background.bmp" );
   bkgd_texture = SDL_CreateTextureFromSurface(Main_Renderer, tmpsurf);
   SDL_FreeSurface(tmpsurf);

   // start our frame update timer
   refresh_timer = SDL_AddTimer(TIMER_UPDATE, update_screen, NULL);

   // start our game timer
   game_timer = SDL_AddTimer(GAME_UPDATE, update_game, NULL);


   // get the audio drivers available
   std::cout << "number of audio drivers available: " << SDL_GetNumAudioDrivers() << std::endl;

   SDL_RendererInfo RenderInfo;

   SDL_GetRendererInfo(Main_Renderer,
      &RenderInfo);

   std::cout << "flags are: " << RenderInfo.flags << std::endl;
    

   while( running )
   {
      while( SDL_PollEvent( &event ) != 0 )
      {
         switch(event.type){
            case (SDL_QUIT):
               running = false;
            case (SDL_KEYDOWN):
               std::cout << "key down is: " <<  event.key.keysym.scancode << std::endl;
               if (event.key.keysym.scancode == 20) running = false;
            default:
               continue;
         }
      }

  }
 }

 // clean up 
 // signal the timer threads to stop running so we can get rid of it
 SDL_SemPost(stop_refresh_timer);
 SDL_SemPost(stop_game_timer);

 // wait until the timer threads have finished the last iteration
 SDL_SemWait(refresh_timer_stopped);
 SDL_SemWait(game_timer_stopped);

 // remove the timers
 SDL_RemoveTimer(refresh_timer);
 SDL_RemoveTimer(game_timer);

 SDL_DestroyTexture(bkgd_texture);
 SDL_DestroyRenderer(Main_Renderer);
 SDL_DestroyWindow( window );
 SDL_Quit();
 return 0;
}
