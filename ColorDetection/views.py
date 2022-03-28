from django.views.decorators.csrf import csrf_exempt
from rest_framework.decorators import api_view
from rest_framework.response import Response
from rest_framework.status import (
    HTTP_400_BAD_REQUEST,
    HTTP_200_OK)
import logging

# Create your views here.

logger = logging.getLogger('iot-seggregator')
logging.basicConfig(level=logging.DEBUG)


@csrf_exempt
@api_view(["POST"])
def process_image(request):
    logger.debug(f"received request : {request.data}")
    response_body = {'status code': HTTP_200_OK,
                     'body': 'dummy response',
                     }
    return Response(response_body, status=HTTP_200_OK)
