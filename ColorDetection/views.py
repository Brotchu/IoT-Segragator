from django.views.decorators.csrf import csrf_exempt
from rest_framework.decorators import api_view
from rest_framework.response import Response
from rest_framework.status import (
    HTTP_400_BAD_REQUEST,
    HTTP_200_OK)
import logging
import io
# import cv2
import numpy as np
from PIL import Image
import mysql.connector
from mysql.connector import errorcode
from sqlalchemy import create_engine
import time, sys
import datetime

# Create your views here.

logger = logging.getLogger('iot-seggregator')
logging.basicConfig(level=logging.DEBUG)

container_map = {
    1: "Red",
    2: "Green",
    3: "Blue"
}


@csrf_exempt
@api_view(["POST"])
def process_image(request):
    # logger.debug(f"received body : {request.body}")
    db_name = "sorting_inventory"
    table_name = "packages_inventory"
    img = Image.open(io.BytesIO(request.body))
    arr_img = np.asarray(img)
    
    # Detect RGB Color
    avg = np.mean(arr_img, axis=1)
    avg = np.mean(avg, axis=0)
    detected_color = avg.argmax() + 1

    logger.info(f"The box is {detected_color}")
    flag = insert_records(db_name, table_name, detected_color)
    return Response(detected_color, status=HTTP_200_OK) 


def insert_records(db_name, table, detected_color):
    return_flag = True
    print(db_name, table)
    cnx = mysql.connector.connect(
            host = "inventory-records.c6xbsgoq927m.us-east-1.rds.amazonaws.com",
            database = db_name,
            user = "admin",
            password = "IoT-Segragator")


    cursor = cnx.cursor()
    ts = time.time()
    timestamp = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
    logger.info(f'timestamp of the identified image {timestamp}')
    try:
        query_builder = f"INSERT INTO {table} (time, container_id, count)  VALUES (\"{timestamp}\", {container_map[detected_color]}, {1});"
        logger.info(f"insert query - {query_builder}")
        cursor.execute(query_builder)
        cnx.commit()
    except:
        logger.error(f"rolling back the query - {sys.exc_info()}")
        cnx.rollback()
        return_flag = False
    cnx.close()
    return return_flag
