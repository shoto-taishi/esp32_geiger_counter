#ifdef _ULPCC_ // do not add code above this line
// must include ulpcc helper functions
#include <ulp_c.h>

// global variable that the main processor can see
unsigned tick_count;

// all ulpcc programs have have this function
void entry()
{
  int previous_state = 1;
  tick_count = 0;
  while (1)
  {
    int current_state = READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 4, 1); // GPIO34 = RTC_GPIO4
    if ((previous_state == 1) && (current_state == 0))
      tick_count++;
    previous_state = current_state;
  }
}
#endif
