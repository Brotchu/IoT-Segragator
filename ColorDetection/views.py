from django.views.decorators.csrf import csrf_exempt
from rest_framework.decorators import api_view
from rest_framework.response import Response
from rest_framework.status import (
    HTTP_400_BAD_REQUEST,
    HTTP_200_OK)
import logging
import io
import cv2
import numpy as np
from PIL import Image

# Create your views here.

logger = logging.getLogger('iot-seggregator')
logging.basicConfig(level=logging.DEBUG)


@csrf_exempt
@api_view(["POST"])
def process_image(request):
    # logger.debug(f"received body : {request.body}")
    img = Image.open(io.BytesIO(request.body))
    arr_img = np.asarray(img)
    rgb_img = cv2.cvtColor(arr_img, cv2.COLOR_BGR2HSV)
    color = {}
    # RED COLOR RANGE
    lower_red = np.array([0, 100, 100])
    upper_red = np.array([7, 255, 255])

    # YELLOW COLOR RANGE
    lower_yellow = np.array([25, 100, 100])
    upper_yellow = np.array([30, 255, 255])

    # GREEN COLOR RANGE
    lower_green = np.array([40, 70, 80])
    upper_green = np.array([70, 255, 255])

    # BLUE COLOR RANGE
    lower_blue = np.array([90, 60, 0])
    upper_blue = np.array([121, 255, 255])

    # THRESHILD
    red = cv2.inRange(rgb_img, lower_red, upper_red)
    green = cv2.inRange(rgb_img, lower_green, upper_green)
    blue = cv2.inRange(rgb_img, lower_blue, upper_blue)
    yellow = cv2.inRange(rgb_img, lower_yellow, upper_yellow)

    # COUNT NUMBER OF WHITE PIXEL
    count_red = np.sum(np.nonzero(red))
    count_green = np.sum(np.nonzero(green))
    count_blue = np.sum(np.nonzero(blue))
    count_yellow = np.sum(np.nonzero(yellow))

    # ADD RESULTS TO DICTIONARY
    color["Red"] = count_red
    color["Green"] = count_green
    color["Blue"] = count_blue
    color["Yellow"] = count_yellow

    logger.info(color)
    detected_color = max(color, key=color.get)
    logger.info(f"The box is {detected_color}")
    response_body = {'status code': HTTP_200_OK,
                     'body': detected_color,
                     }
    return Response(response_body, status=HTTP_200_OK)
