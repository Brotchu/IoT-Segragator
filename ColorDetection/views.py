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
import mysql.connector
from mysql.connector import errorcode
from sqlalchemy import create_engine
import time, sys
import datetime
import cv2
from sklearn.cluster import KMeans
from collections import Counter

# Create your views here.

logger = logging.getLogger('iot-seggregator')
logging.basicConfig(level=logging.DEBUG)

container_map = {
    1: "Blue",
    2: "Green",
    3: "Red"
}

def get_image(img):
    arr_img = np.asarray(img)
    rgb_img = cv2.cvtColor(arr_img, cv2.COLOR_BGR2RGB)
    return rgb_img


def get_colours(image, max_value):
    img = cv2.resize(image, (600, 400))
    img = img.reshape(img.shape[0] * img.shape[1], 3)
    cluster = KMeans(n_clusters=max_value)
    labels = cluster.fit_predict(img)
    ct = Counter(labels)
    center = cluster.cluster_centers_
    order = [center[i] for i in ct.keys()]
    top_detected_color = [rgb.tolist() for rgb in order]
    for color in top_detected_color:
        logger.info(f"[+] {color}")
    return top_detected_color


@csrf_exempt
@api_view(["POST"])
def process_image(request):
    # logger.debug(f"received body : {request.body}")
    db_name = "sorting_inventory"
    table_name = "packages_inventory"
    max_allowed_color = 5
    img = Image.open(io.BytesIO(request.body))
    rgb_img = get_image(img)
    color_detected = get_colours(rgb_img, max_allowed_color)
    final_color = max([color.index(max(color)) for color in color_detected])
    color_code = container_map[final_color + 1]
    logger.info(f"{final_color} - {color_code}")
    flag = insert_records(db_name, table_name, color_code)
    return Response(color_code, status=HTTP_200_OK)


def insert_records(db_name, table, detected_color):
    return_flag = True
    print(db_name, table)
    cnx = mysql.connector.connect(
        host="inventory-records.c6xbsgoq927m.us-east-1.rds.amazonaws.com",
        database=db_name,
        user="admin",
        password="IoT-Segragator")

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
