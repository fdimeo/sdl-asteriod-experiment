/*
SDL2_test.c
Testing SDL2 with Linux 
http://nothingtocode.blogspot.com
*/

#include <SDL2/SDL.h>
#include <cstdbool>
#include <cstdlib>
#include <iostream>
#include <unordered_set>
#include <utility>

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
typedef std::pair<int, int> position;
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
   position getPosition();
   void render(SDL_Renderer *pRenderer);
};


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

position asteroid::getPosition()
{
   return (std::make_pair(dstrect.x, dstrect.y));
}


class all_asteroids {
private:
   std::unordered_set<asteroid *> m_all_asteroids;  // a set of all the asteroids alive in the game
   std::pair<unsigned int, unsigned int> m_window_boundaries;  // the boundaries for the asteroids

public:
   all_asteroids(std::pair<unsigned int, unsigned int> window_boundaries);
   void spawn();
   void update();     
   ~all_asteroids();  // delete all asteroids created on the heap
};

all_asteroids::all_asteroids(std::pair<unsigned int, unsigned int> window_boundaries):m_window_boundaries(window_boundaries)
{
}

void all_asteroids::spawn()
{
   if (m_all_asteroids.size() < MAX_NUM_ASTEROIDS)
   {
      srand (time(NULL));

      double speedx = ((rand() % 9)+1)/4.0;
      double speedy = -1.0 * ((rand() % 9)+1)/4.0;
   
      std::cout << "speedx is: " << speedx << " and speedy is: " << speedy << std::endl;
  
      m_all_asteroids.insert(new asteroid(std::make_pair((rand() % m_window_boundaries.first),(rand() % m_window_boundaries.second)), 
            std::make_pair(speedx, speedy), 
            1.2, 
            (rand() % 1) ? direction::RIGHT : direction::LEFT));
   }
}

// updates and renders all asteroids that are currently alive
void all_asteroids::update()
{
   position pos;
   std::unordered_set<asteroid *> tmp_set;
   

   for (auto it : m_all_asteroids){
      it->update();
      pos = it->getPosition();
      if ((pos.first > m_window_boundaries.first) || (pos.second > m_window_boundaries.second) || !(pos.first > 0) || !(pos.second > 0) ){
         tmp_set.insert(it);
      }
      else{
         it->render(Main_Renderer);
      }
   }

   // remove any asteroids that have gone out of scope
   for( auto it : tmp_set){
      std::cout << "erasing asteroid" << std::endl;
      m_all_asteroids.erase(it);
   }
}

all_asteroids::~all_asteroids()
{
   std::cout << "whacking all spawned asteroids" << std::endl;
   // wipe out any and all asteroids created on the heap
   m_all_asteroids.clear();
}


Uint32 update_game(Uint32 interval, void *param)
{
   // run the timer until the main thread signals up to stop
   if(!SDL_SemValue(stop_game_timer)){

      all_asteroids *pAsteroids = (reinterpret_cast< all_asteroids* > (param));

      pAsteroids->spawn();
      
   }

   else{
      std::cout << "game timer stopped" << std::endl;

      // we've been requested to stop this timer...so tell the main thread that we have
      SDL_SemPost(game_timer_stopped);
   }

   return interval;

}


Uint32 update_screen(Uint32 interval, void *param)
{
   // run the timer until the main thread signals up to stop
   if(!SDL_SemValue(stop_refresh_timer)){

      all_asteroids *pAsteroids = (reinterpret_cast< all_asteroids* > (param));

      SDL_RenderCopy(Main_Renderer, bkgd_texture, NULL, NULL);
   
      // update all the game asteroids
      pAsteroids->update();

      SDL_RenderPresent(Main_Renderer);
   }
   else{
      //      std::cout << "refresh timer stopped" << std::endl;
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

      // create the game asteroids instance, and tell it where the window boundaries are
      all_asteroids game_asteroids(std::make_pair(WINDOW_WIDTH, WINDOW_HEIGHT));

      std::cout << "updating screen every " << TIMER_UPDATE << " mS" << std::endl;
   
      Main_Renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

      SDL_Surface *tmpsurf = SDL_LoadBMP( "background.bmp" );
      bkgd_texture = SDL_CreateTextureFromSurface(Main_Renderer, tmpsurf);
      SDL_FreeSurface(tmpsurf);

      // start our frame update timer
      refresh_timer = SDL_AddTimer(TIMER_UPDATE, update_screen, &game_asteroids);

      // start our game timer and pass the asteroids instance
      game_timer = SDL_AddTimer(GAME_UPDATE, update_game, &game_asteroids);


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
