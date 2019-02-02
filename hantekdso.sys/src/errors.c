/*
 * errors.c
 *
 *  Created on: 30 янв. 2019 г.
 *      Author: ugnenko
 */


#include "errors.h"


static const char * dso_error_names[] = {
    FOREACH_DSOERROR(GENERATE_STRING)
};

const char *
dso_error_name (int dso_error_code)
{
  return dso_error_names[dso_error_code];
}
